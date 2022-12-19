#include "thread.hpp"

#include "bsp_time.h"
#include "webots/robot.h"
#include "webots/supervisor.h"

using namespace System;

static bool simulating = false;

void Thread::StartKernel() {
  while (1) {
    simulating = true;
    wb_robot_step(1);
    simulating = false;
    poll(NULL, 0, 2);
  }
}

void Thread::Sleep(uint32_t microseconds) {
  float time = bsp_time_get() + microseconds / 1000.0f;

  while (simulating || bsp_time_get() < time) {
    poll(NULL, 0, 1);
  }
}

void Thread::SleepUntil(uint32_t microseconds) {
  float time = bsp_time_get() + microseconds / 1000.0f;

  while (simulating || bsp_time_get() < time) {
    poll(NULL, 0, 1);
  }
}
