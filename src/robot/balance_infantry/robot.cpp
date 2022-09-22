#include "robot.hpp"
/* clang-format off */
Robot::Infantry::Param param = {
    .chassis={
      .l1 = 0.11f,
      .l2 = 0.2f,
      .l3 = 0.31f,

      .leg_max_angle = 0.0f,

      .mech_zero = {
        0.630508065f,
        5.75973034f,
        1.45487654f,
        3.00471401f,
      },

      .leg_actr = {
        {
        .speed = {
          .k = 1.0f,
          .p = 1.0f,
          .i = 1.2f,
          .d = 0.0f,
          .i_limit = 3.0f,
          .out_limit = 3.0f,
          .d_cutoff_freq = -1.0f,
          .range = -1.0f,
        },

        .position = {
          .k = 20.0f,
          .p = 1.0f,
          .i = 0.3f,
          .d = 0.0f,
          .i_limit = 0.0f,
          .out_limit = 10.0f,
          .d_cutoff_freq = -1.0f,
          .range = M_2PI,
        },

        .in_cutoff_freq = -1.0f,

        .out_cutoff_freq = -1.0f,
        },{
        .speed = {
          .k = 1.0f,
          .p = 1.0f,
          .i = 1.2f,
          .d = 0.0f,
          .i_limit = 3.0f,
          .out_limit = 3.0f,
          .d_cutoff_freq = -1.0f,
          .range = -1.0f,
        },

        .position = {
          .k = 20.0f,
          .p = 1.0f,
          .i = 0.3f,
          .d = 0.0f,
          .i_limit = 0.0f,
          .out_limit = 10.0f,
          .d_cutoff_freq = -1.0f,
          .range = M_2PI,
        },

        .in_cutoff_freq = -1.0f,

        .out_cutoff_freq = -1.0f,
        },{
        .speed = {
          .k = 1.0f,
          .p = 1.0f,
          .i = 1.2f,
          .d = 0.0f,
          .i_limit = 3.0f,
          .out_limit = 3.0f,
          .d_cutoff_freq = -1.0f,
          .range = -1.0f,
        },

        .position = {
          .k = 20.0f,
          .p = 1.0f,
          .i = 0.3f,
          .d = 0.0f,
          .i_limit = 0.0f,
          .out_limit = 10.0f,
          .d_cutoff_freq = -1.0f,
          .range = M_2PI,
        },

        .in_cutoff_freq = -1.0f,

        .out_cutoff_freq = -1.0f,
        },{
        .speed = {
          .k = 1.0f,
          .p = 1.0f,
          .i = 1.2f,
          .d = 0.0f,
          .i_limit = 3.0f,
          .out_limit = 3.0f,
          .d_cutoff_freq = -1.0f,
          .range = -1.0f,
        },

        .position = {
          .k = 20.0f,
          .p = 1.0f,
          .i = 0.3f,
          .d = 0.0f,
          .i_limit = 0.0f,
          .out_limit = 10.0f,
          .d_cutoff_freq = -1.0f,
          .range = M_2PI,
        },

        .in_cutoff_freq = -1.0f,

        .out_cutoff_freq = -1.0f,
        },
      },

      .leg_motor = {
        {
          .kp = 30.0f,
          .kd = 0.1f,
          .def_speed = 0.0f,
          .id = 1,
          .can = BSP_CAN_1,
          .max_error = 0.1f,
        },
        {
          .kp = 30.0f,
          .kd = 0.1f,
          .def_speed = 0.0f,
          .id = 2,
          .can = BSP_CAN_1,
          .max_error = 0.1f,
        },
        {
          .kp = 30.0f,
          .kd = 0.1f,
          .def_speed = 0.0f,
          .id = 3,
          .can = BSP_CAN_1,
          .max_error = 0.1f,
        },
        {
          .kp = 30.0f,
          .kd = 0.1f,
          .def_speed = 0.0f,
          .id = 4,
          .can = BSP_CAN_1,
          .max_error = 0.1f,
        },
      },
  },

  .bmi088_rot = {
    .rot_mat = {
      { +1, +0, +0},
      { +0, +1, +0},
      { +0, +0, +1},
    },
  },

  .bmi088_cali = {
    .gyro_offset = {
      .x = 0.000332771f,
      .y = 0.004123951f,
      .z = -0.000634991f,
    },
  },

  .cap = {
    .can = BSP_CAN_1,
    .index = DEV_CAP_FB_ID_BASE,
  },
};
/* clang-format on */

void robot_init() {
  auto init_thread_fn = [](void* arg) {
    RM_UNUSED(arg);

    Robot::Infantry robot(param, 500.0f);
    while (1) {
      System::Thread::Sleep(UINT32_MAX);
    }
  };

  System::Thread init_thread;
  THREAD_DECLEAR(init_thread, init_thread_fn, 1024, System::Thread::Realtime,
                 NULL);
}