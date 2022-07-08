/**
 * @file chassis.c
 * @author Qu Shen (503578404@qq.com)
 * @brief 底盘模组
 * @version 1.0.0
 * @date 2021-04-15
 *
 * @copyright Copyright (c) 2021
 *
 */

#include "mod_chassis.hpp"

#include <stdlib.h>

#include "FreeRTOS.h"
#include "comp_limiter.hpp"
#include "dev_can.hpp"
#include "dev_cap.hpp"
#include "dev_tof.hpp"

#define ROTOR_WZ_MIN 0.6f /* 小陀螺旋转位移下界 */
#define ROTOR_WZ_MAX 0.8f /* 小陀螺旋转位移上界 */

#define ROTOR_OMEGA 0.0015f /* 小陀螺转动频率 */

#define SCAN_VY_LENGTH_MIN 0.3f /* 哨兵返回时距离边界的最小值 */
#define SCAN_MOVEMENTS 0.6f     /* 哨兵移动速度 */

#define MOTOR_MAX_ROTATIONAL_SPEED 7000.0f /* 电机的最大转速 */

#if POWER_LIMIT_WITH_CAP
/* 保证电容电量宏定义在正确范围内 */
#if ((CAP_PERCENT_NO_LIM < 0) || (CAP_PERCENT_NO_LIM > 100) || \
     (CAP_PERCENT_WORK < 0) || (CAP_PERCENT_WORK > 100))
#error "Cap percentage should be in the range from 0 to 100."
#endif

/* 保证电容功率宏定义在正确范围内 */
#if ((CAP_MAX_LOAD < 60) || (CAP_MAX_LOAD > 200))
#error "The capacitor power should be in in the range from 60 to 200."
#endif

static const float kCAP_PERCENTAGE_NO_LIM = (float)CAP_PERCENT_NO_LIM / 100.0f;
static const float kCAP_PERCENTAGE_WORK = (float)CAP_PERCENT_WORK / 100.0f;

#endif

bool motor_ref_data_valid(const referee_for_chassis_t *ref) {
  return (ref->chassis_power_limit > 0.0f) && (ref->chassis_pwr_buff > 0.0f) &&
         (ref->chassis_watt > 0.0f);
}

/**
 * @brief 设置底盘模式
 *
 * @param c 包含底盘数据的结构体
 * @param mode 要设置的模式
 */
static void chassis_set_mode(chassis_t *c, chassis_mode_t mode, uint32_t now) {
  ASSERT(c);                   /* 主结构体不能为空 */
  if (mode == c->mode) return; /* 模式未改变直接返回 */

  if (mode == CHASSIS_MODE_SCAN && c->mode != CHASSIS_MODE_SCAN) {
    c->vy_dir_mult = (rand() % 2) ? -1 : 1;
  }

  if (mode == CHASSIS_MODE_ROTOR && c->mode != CHASSIS_MODE_ROTOR) {
    srand(now);
    c->wz_dir_mult = (rand() % 2) ? -1 : 1;
  }
  /* 切换模式后重置PID和滤波器 */
  for (size_t i = 0; i < c->num_wheel; i++) {
    kpid_reset(c->pid.motor + i);
    low_pass_filter_2p_reset(c->filter.in + i, 0.0f);
    low_pass_filter_2p_reset(c->filter.out + i, 0.0f);
  }
  c->mode = mode;
}
/**
 * @brief 计算小陀螺wz速度
 *
 * @param lo wz最小速度
 * @param hi wz最大速度
 * @param now ctrl_chassis的tick数
 * @return float wz
 */
static float chassis_calc_wz(const float lo, const float hi, uint32_t now) {
  float wz_vary = fabsf(0.2f * sinf(ROTOR_OMEGA * (float)now)) + lo;
  clampf(&wz_vary, lo, hi);
  return wz_vary;
}

/**
 * @brief 初始化底盘
 *
 * @param c 包含底盘数据的结构体
 * @param param 包含底盘参数的结构体指针
 * @param target_freq 任务预期的运行频率
 */
void chassis_init(chassis_t *c, const chassis_params_t *param,
                  eulr_t *gimbal_mech_zero, float target_freq) {
  ASSERT(c);

  c->param = param;             /* 初始化参数 */
  c->mode = CHASSIS_MODE_RELAX; /* 设置上电后底盘默认模式 */
  c->gimbal_mech_zero = gimbal_mech_zero; /* 设置底盘机械零点 */

  /* 如果电机反装重新计算机械零点 */
  if (param->reverse.yaw) circle_reverse(&(c->gimbal_mech_zero->yaw));

  /* 根据参数（param）中的底盘型号初始化Mixer */
  mixer_mode_t mixer_mode = MIXER_SINGLE;
  switch (c->param->type) {
    case CHASSIS_TYPE_MECANUM:
      c->num_wheel = 4;
      mixer_mode = MIXER_MECANUM;
      break;

    case CHASSIS_TYPE_PARLFIX4:
      c->num_wheel = 4;
      mixer_mode = MIXER_PARLFIX4;
      break;

    case CHASSIS_TYPE_PARLFIX2:
      c->num_wheel = 2;
      mixer_mode = MIXER_PARLFIX2;
      break;

    case CHASSIS_TYPE_OMNI_CROSS:
      c->num_wheel = 4;
      mixer_mode = MIXER_OMNICROSS;
      break;

    case CHASSIS_TYPE_OMNI_PLUS:
      c->num_wheel = 4;
      mixer_mode = MIXER_OMNIPLUS;
      break;

    case CHASSIS_TYPE_SINGLE:
      c->num_wheel = 1;
      mixer_mode = MIXER_SINGLE;
      break;
    case CHASSIS_TYPE_NONE:
      c->num_wheel = 0;
      mixer_mode = MIXER_NONE;
      return;

    case CHASSIS_TYPE_DRONE:
      /* onboard sdk. */
      ASSERT(0);
      break;

    default:
      ASSERT(0);
  }

  /* 根据底盘型号动态分配控制时使用的变量 */
  c->feedback.motor_rotational_speed = (float *)pvPortMalloc(
      c->num_wheel * sizeof(*c->feedback.motor_rotational_speed));
  ASSERT(c->feedback.motor_rotational_speed);

  c->setpoint.motor_rotational_speed = (float *)pvPortMalloc(
      c->num_wheel * sizeof(*c->setpoint.motor_rotational_speed));
  ASSERT(c->setpoint.motor_rotational_speed);

  c->pid.motor = (kpid_t *)pvPortMalloc(c->num_wheel * sizeof(*c->pid.motor));
  ASSERT(c->pid.motor);

  c->filter.in = (low_pass_filter_2p_t *)pvPortMalloc(c->num_wheel *
                                                      sizeof(*c->filter.in));
  ASSERT(c->filter.in);

  c->filter.out = (low_pass_filter_2p_t *)pvPortMalloc(c->num_wheel *
                                                       sizeof(*c->filter.out));
  ASSERT(c->filter.out);

  /* 初始化轮子电机控制PID和LPF */
  for (size_t i = 0; i < c->num_wheel; i++) {
    kpid_init(c->pid.motor + i, KPID_MODE_NO_D, KPID_MODE_K_SET, target_freq,
              &(c->param->motor_pid_param));

    low_pass_filter_2p_init(c->filter.in + i, target_freq,
                            c->param->low_pass_cutoff_freq.in);
    low_pass_filter_2p_init(c->filter.out + i, target_freq,
                            c->param->low_pass_cutoff_freq.out);
  }

  /* 初始化跟随云台的控制PID */
  kpid_init(&(c->pid.follow), KPID_MODE_NO_D, KPID_MODE_K_SET, target_freq,
            &(c->param->follow_pid_param));

  mixer_init(&(c->mixer), mixer_mode); /* 初始化混合器 */
}

/**
 * @brief 更新底盘的反馈信息
 *
 * @param c 包含底盘数据的结构体
 * @param chassis_motor 底盘电机反馈
 * @param gimbal_motor 云台电机反馈
 * @param tof 距离传感器反馈
 */
void chassis_update_feedback(chassis_t *c,
                             const motor_feedback_group_t *chassis_motor,
                             const motor_feedback_group_t *gimbal_motor,
                             const tof_feedback_t *tof) {
  /* 底盘数据和CAN结构体不能为空 */
  ASSERT(c);
  ASSERT(chassis_motor);
  ASSERT(gimbal_motor);

  /* 如果yaw云台电机反装重新计算正确的反馈值 */
  c->feedback.gimbal_yaw_encoder_angle =
      gimbal_motor->as_gimbal_yaw.yaw.rotor_abs_angle;
  if (c->param->reverse.yaw)
    circle_reverse(&(c->feedback.gimbal_yaw_encoder_angle));

  /* 将CAN中的反馈数据写入到feedback中 */
  for (size_t i = 0; i < c->num_wheel; i++) {
    c->feedback.motor_rotational_speed[i] =
        chassis_motor->as_array[i].rotational_speed;
  }

  /* 更新距离传感器数据 */
  memcpy(&(c->feedback.tof), tof, sizeof(tof_feedback_t));
}

/**
 * @brief 运行底盘控制逻辑
 *
 * @param c 包含底盘数据的结构体
 * @param c_cmd 底盘控制指令
 * @param dt_sec 两次调用的时间间隔
 */
void chassis_control(chassis_t *c, const cmd_chassis_t *c_cmd, uint32_t now) {
  /* 底盘数据和控制指令结构体不能为空 */
  ASSERT(c);
  ASSERT(c_cmd);

  c->dt = (float)(now - c->lask_wakeup) / 1000.0f;
  c->lask_wakeup = now;

  /* 根据遥控器命令更改底盘模式 */
  chassis_set_mode(c, c_cmd->mode, now);
  /* ctrl_vec -> move_vec 控制向量和真实的移动向量之间有一个换算关系 */
  /* 计算vx、vy */
  switch (c->mode) {
    case CHASSIS_MODE_BREAK: /* 刹车模式电机停止 */
      c->move_vec.vx = 0.0f;
      c->move_vec.vy = 0.0f;
      break;

    case CHASSIS_MODE_INDENPENDENT: /* 独立模式控制向量与运动向量相等 */
      c->move_vec.vx = c_cmd->ctrl_vec.vx;
      c->move_vec.vy = c_cmd->ctrl_vec.vy;
      break;

    case CHASSIS_MODE_OPEN:
    case CHASSIS_MODE_RELAX:
    case CHASSIS_MODE_FOLLOW_GIMBAL: /* 按照云台方向换算运动向量 */
    case CHASSIS_MODE_FOLLOW_GIMBAL_35:
    case CHASSIS_MODE_ROTOR: {
      float beta =
          c->feedback.gimbal_yaw_encoder_angle - c->gimbal_mech_zero->yaw;
      float cos_beta = cosf(beta);
      float sin_beta = sinf(beta);
      c->move_vec.vx =
          cos_beta * c_cmd->ctrl_vec.vx - sin_beta * c_cmd->ctrl_vec.vy;
      c->move_vec.vy =
          sin_beta * c_cmd->ctrl_vec.vx + cos_beta * c_cmd->ctrl_vec.vy;
      break;
    }
    case CHASSIS_MODE_SCAN:
      /* 根据距离传感器数据变向 */
      if (c->feedback.tof.data[DEV_TOF_SENSOR_LEFT].dist < SCAN_VY_LENGTH_MIN)
        c->vy_dir_mult = 1;
      else if (c->feedback.tof.data[DEV_TOF_SENSOR_RIGHT].dist <
               SCAN_VY_LENGTH_MIN)
        c->vy_dir_mult = -1;
      c->move_vec.vy = c->vy_dir_mult * SCAN_MOVEMENTS;
      break;
  }

  /*哨兵底盘电机反装*/
  if (c->param->reverse.chassis_motor) {
    c->move_vec.vy = -c->move_vec.vy;
    c->move_vec.vx = -c->move_vec.vx;
  }

  /* 计算wz */
  switch (c->mode) {
    case CHASSIS_MODE_RELAX:
    case CHASSIS_MODE_BREAK:
    case CHASSIS_MODE_INDENPENDENT: /* 独立模式wz为0 */
      c->move_vec.wz = 0.0f;
      break;

    case CHASSIS_MODE_OPEN:
    case CHASSIS_MODE_FOLLOW_GIMBAL: /* 跟随模式通过PID控制使车头跟随云台 */
      c->move_vec.wz =
          kpid_calc(&(c->pid.follow), c->gimbal_mech_zero->yaw,
                    c->feedback.gimbal_yaw_encoder_angle, 0.0f, c->dt);
      break;
    case CHASSIS_MODE_FOLLOW_GIMBAL_35:
      c->move_vec.wz = kpid_calc(
          &(c->pid.follow), c->gimbal_mech_zero->yaw + (35.0f * M_DEG2RAD_MULT),
          c->feedback.gimbal_yaw_encoder_angle, 0.0f, c->dt);
      break;
    case CHASSIS_MODE_ROTOR: { /* 小陀螺模式使底盘以一定速度旋转 */
      c->move_vec.wz =
          c->wz_dir_mult * chassis_calc_wz(ROTOR_WZ_MIN, ROTOR_WZ_MAX, now);
    }
    case CHASSIS_MODE_SCAN:
      break;
  }

  /* move_vec -> motor_rpm_set. 通过运动向量计算轮子转速目标值 */
  mixer_apply(&(c->mixer), &(c->move_vec), c->setpoint.motor_rotational_speed,
              c->num_wheel, MOTOR_MAX_ROTATIONAL_SPEED);

  /* 根据轮子转速目标值，利用PID计算电机输出值 */
  for (size_t i = 0; i < c->num_wheel; i++) {
    /* 输入滤波. */
    c->feedback.motor_rotational_speed[i] = low_pass_filter_2p_apply(
        c->filter.in + i, c->feedback.motor_rotational_speed[i]);

    /* 根据底盘模式计算输出值 */
    switch (c->mode) {
      case CHASSIS_MODE_BREAK:
      case CHASSIS_MODE_FOLLOW_GIMBAL:
      case CHASSIS_MODE_FOLLOW_GIMBAL_35:
      case CHASSIS_MODE_ROTOR:
      case CHASSIS_MODE_SCAN:
      case CHASSIS_MODE_INDENPENDENT: /* 独立模式,受PID控制 */
        c->out.motor.as_array[i] =
            kpid_calc(c->pid.motor + i, c->setpoint.motor_rotational_speed[i],
                      c->feedback.motor_rotational_speed[i], 0.0f, c->dt);
        break;

      case CHASSIS_MODE_OPEN: /* 开环模式,不受PID控制 */
        c->out.motor.as_array[i] =
            c->setpoint.motor_rotational_speed[i] / MOTOR_MAX_ROTATIONAL_SPEED;
        break;

      case CHASSIS_MODE_RELAX: /* 放松模式,不输出 */
        c->out.motor.as_array[i] = 0;
        break;
    }
    /* 输出滤波. */
    c->out.motor.as_array[i] =
        low_pass_filter_2p_apply(c->filter.out + i, c->out.motor.as_array[i]);
  }
}

/**
 * @brief 底盘功率限制
 *
 * @param c 底盘数据
 * @param cap 电容数据
 * @param ref 裁判系统数据
 */
void chassis_power_limit(chassis_t *c, const cap_feedback_t *cap,
                         const referee_for_chassis_t *ref) {
  ASSERT(c);
  ASSERT(cap);
  ASSERT(ref);

#if POWER_UNLIMIT || DEBUG_POWER_UNLIMIT
  return;
#endif

  float power_limit = limit_calc_chassic_output_power(ref->chassis_power_limit,
                                                      ref->chassis_pwr_buff);

  if (ref->status != REF_STATUS_RUNNING) {
    /* 裁判系统离线，将功率限制为固定值 */
    power_limit = GAME_CHASSIS_MAX_POWER_WO_REF;
  }
#if POWER_LIMIT_WITH_CAP
  else {
    if (cap->percentage > kCAP_PERCENTAGE_WORK) {
      /* 按照电容能量百分比计算输出功率 */
      power_limit = ref->chassis_power_limit +
                    (cap->percentage - kCAP_PERCENTAGE_WORK) /
                        (kCAP_PERCENTAGE_NO_LIM - kCAP_PERCENTAGE_WORK) *
                        (float)CAP_MAX_LOAD;
    } else {
      /* 电容不在工作，根据缓冲能量计算输出功率限制 */
      power_limit = limit_calc_chassic_output_power(ref->chassis_power_limit,
                                                    ref->chassis_pwr_buff);
    }
  }
#endif
  /* 应用功率限制 */
  limit_chassic_output_power(power_limit, c->out.motor.as_array,
                             c->feedback.motor_rotational_speed, c->num_wheel);

  if (!motor_ref_data_valid(ref)) {
    /* 当裁判系统离线时，依然使用裁判系统进程传来的数据 */
    c->out.cap.power_limit = ref->chassis_power_limit;
  } else {
    /* 当裁判系统在线时，使用算法控制裁判系统输出（即电容输入） */
    c->out.cap.power_limit = limit_cap_input_power(
        ref->chassis_watt, ref->chassis_power_limit, ref->chassis_pwr_buff);
  }
}

/**
 * @brief 复制底盘输出值
 *
 * @param c 包含底盘数据的结构体
 * @param out CAN设备底盘输出结构体
 */
void chassis_pack_output(const chassis_t *c, motor_control_t *motor,
                         cap_control_t *cap) {
  ASSERT(c);
  ASSERT(motor);
  ASSERT(cap);
  memcpy(motor, &(c->out.motor), sizeof(*motor));
  memcpy(cap, &(c->out.cap), sizeof(*cap));
}

/**
 * @brief 导出底盘数据
 *
 * @param chassis 底盘数据结构体
 * @param ui UI数据结构体
 */
void chassis_pack_ui(const chassis_t *c, ui_chassis_t *ui) {
  ASSERT(c);
  ASSERT(ui);
  ui->mode = c->mode;
  ui->angle = c->feedback.gimbal_yaw_encoder_angle - c->gimbal_mech_zero->yaw;
}