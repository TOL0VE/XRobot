/*
  初始化任务

  根据机器人的FLASH配置，决定生成哪些任务。
*/

/* Includes ----------------------------------------------------------------- */
#include "bsp\flash.h"
#include "bsp\usb.h"
#include "component\cmd.h"
#include "device\bmi088.h"
#include "device\ist8310.h"
#include "device\referee.h"
#include "module\chassis.h"
#include "module\gimbal.h"
#include "module\shoot.h"
#include "task\user_task.h"

/* Private typedef ---------------------------------------------------------- */
/* Private define ----------------------------------------------------------- */
/* Private macro ------------------------------------------------------------ */
/* Private variables -------------------------------------------------------- */
/* Private function --------------------------------------------------------- */
/* Exported functions ------------------------------------------------------- */

/**
 * \brief 初始化
 *
 * \param argument 未使用
 */
void Task_Init(void *argument) {
  (void)argument; /* 未使用argument，消除警告 */

  Config_Get(&task_runtime.cfg); /* 获取机器人配置 */

  osKernelLock();
  /* 创建任务 */
  task_runtime.thread.atti_esti =
      osThreadNew(Task_AttiEsti, NULL, &attr_atti_esti);
  task_runtime.thread.cli = osThreadNew(Task_CLI, NULL, &attr_cli);
  task_runtime.thread.command = osThreadNew(Task_Command, NULL, &attr_command);
  task_runtime.thread.ctrl_chassis =
      osThreadNew(Task_CtrlChassis, NULL, &attr_ctrl_chassis);
  task_runtime.thread.ctrl_gimbal =
      osThreadNew(Task_CtrlGimbal, NULL, &attr_ctrl_gimbal);
  task_runtime.thread.ctrl_shoot =
      osThreadNew(Task_CtrlShoot, NULL, &attr_ctrl_shoot);
  task_runtime.thread.info = osThreadNew(Task_Info, NULL, &attr_info);
  task_runtime.thread.monitor = osThreadNew(Task_Monitor, NULL, &attr_monitor);
  task_runtime.thread.can = osThreadNew(Task_Can, NULL, &attr_can);
  task_runtime.thread.referee = osThreadNew(Task_Referee, NULL, &attr_referee);
  task_runtime.thread.ai = osThreadNew(Task_Ai, NULL, &attr_ai);
  task_runtime.thread.rc = osThreadNew(Task_RC, NULL, &attr_rc);
  task_runtime.thread.cap = osThreadNew(Task_Cap, NULL, &attr_cap);

  /* 创建消息队列 */
  /* motor */
  task_runtime.msgq.can.feedback.chassis =
      osMessageQueueNew(6u, sizeof(CAN_t), NULL);
  task_runtime.msgq.can.feedback.gimbal =
      osMessageQueueNew(6u, sizeof(CAN_t), NULL);
  task_runtime.msgq.can.feedback.shoot =
      osMessageQueueNew(6u, sizeof(CAN_t), NULL);
  task_runtime.msgq.can.feedback.cap =
      osMessageQueueNew(6u, sizeof(CAN_t), NULL);
  task_runtime.msgq.can.output.chassis =
      osMessageQueueNew(6u, sizeof(CAN_ChassisOutput_t), NULL);
  task_runtime.msgq.can.output.gimbal =
      osMessageQueueNew(6u, sizeof(CAN_GimbalOutput_t), NULL);
  task_runtime.msgq.can.output.shoot =
      osMessageQueueNew(6u, sizeof(CAN_ShootOutput_t), NULL);
  task_runtime.msgq.can.output.cap =
      osMessageQueueNew(6u, sizeof(CAN_CapOutput_t), NULL);

  /* command */
  task_runtime.msgq.cmd.chassis =
      osMessageQueueNew(3u, sizeof(CMD_ChassisCmd_t), NULL);
  task_runtime.msgq.cmd.gimbal =
      osMessageQueueNew(3u, sizeof(CMD_GimbalCmd_t), NULL);
  task_runtime.msgq.cmd.shoot =
      osMessageQueueNew(3u, sizeof(CMD_ShootCmd_t), NULL);
  task_runtime.msgq.cmd.ai =
      osMessageQueueNew(6u, sizeof(CMD_AI_Status_t), NULL);
  task_runtime.msgq.cmd.referee = osMessageQueueNew(6u, sizeof(CMD_UI_t), NULL);

  /* atti_esti */
  task_runtime.msgq.cmd.raw.rc = osMessageQueueNew(3u, sizeof(CMD_RC_t), NULL);
  task_runtime.msgq.cmd.raw.host =
      osMessageQueueNew(3u, sizeof(CMD_Host_t), NULL);

  task_runtime.msgq.gimbal.accl =
      osMessageQueueNew(6u, sizeof(AHRS_Accl_t), NULL);
  task_runtime.msgq.gimbal.eulr_imu =
      osMessageQueueNew(6u, sizeof(AHRS_Eulr_t), NULL);
  task_runtime.msgq.gimbal.gyro =
      osMessageQueueNew(6u, sizeof(AHRS_Gyro_t), NULL);

  task_runtime.msgq.referee = osMessageQueueNew(6u, sizeof(Referee_t), NULL);
  task_runtime.msgq.cap_info =
      osMessageQueueNew(6u, sizeof(CAN_Capacitor_t), NULL);

  /* AI */
  task_runtime.msgq.ai.quat =
      osMessageQueueNew(6u, sizeof(AHRS_Quaternion_t), NULL);
  task_runtime.msgq.ai.referee = osMessageQueueNew(6u, sizeof(Referee_t), NULL);

  osKernelUnlock();

  osThreadTerminate(osThreadGetId()); /* 结束自身 */
}
