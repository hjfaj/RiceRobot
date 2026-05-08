#include "Delay.h"
#include "ESP8266.h"
#include "HCSR04.h"
#include "HX711.h"
#include "Key.h"
#include <math.h>
#include "L298N.h"
#include "L298N_Cutter.h"
#include "MPU6050.h"
#include "OLED.h"
#include "Sensor.h"
#include "SU03T.h"
#include "WINDOW.h"
#include "Buzzer.h"
#include "stm32f10x.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * ============================================================
 * 传感器数据单位说明
 * ============================================================
 * weight      : 重量 (单位: 千克 kg)
 * speed       : 速度 (单位: 米/秒 m/s, 从电机PWM估算)
 * storage_full: 存储仓状态 (0-未满, 1-已满)
 * roll        : 横滚角 (单位: 度 °)
 * pitch       : 俯仰角 (单位: 度 °)
 * ============================================================
 */

static int windows = 1;

static float weight = 0.0f;
static float speed = 0.0f;
static uint8_t storage_full = 0;
static uint16_t dist_left = 0;
static uint16_t dist_right = 0;

static uint8_t saw_running = 0;
static uint8_t conveyor_running = 0;

// TURN_180 非阻塞状态机
static uint8_t turning_180 = 0;
static uint32_t turn_start_tick = 0;
#define TURN_180_DURATION_MS 800

// 保存用户通过 CMD:SPEED 设定的速度，防止被差分补偿覆盖
static uint8_t user_motor_speed = 30;

static float roll = 0.0f;
static float pitch = 0.0f;
static Stability_State stability_state = STABILITY_NORMAL;
static uint8_t mpu6050_online = 0;

static float last_weight = -1.0f;
static float last_speed = -1.0f;
static uint16_t last_dist_left = 0xFFFF;
static uint16_t last_dist_right = 0xFFFF;
static float last_roll = -100.0f;
static float last_pitch = -100.0f;
static int last_windows = -1;
static Stability_State last_stability_state = STABILITY_NORMAL;
static uint8_t last_mpu6050_online = 0xFF;

void Window_Next(void) {
  if (windows < 4) {
    windows++;
  } else {
    windows = 1;
  }
  Window_Show(windows);
}

void Window_Previous(void) {
  if (windows > 1) {
    windows--;
  } else {
    windows = 4;
  }
  Window_Show(windows);
}

float EstimateSpeedFromPWM(void) {
  return (float)g_motor_speed / 100.0f * 2.0f;
}

void ReadSensors(void) {
  weight = HX711_Get_Weight() / 1000.0f;
  if (weight < 0.02f)
    weight = 0;

  speed = EstimateSpeedFromPWM();

  storage_full = Sensor_GetLimitSwitch() || Sensor_GetIR() ||
                 (weight >= STORAGE_WEIGHT_THRESHOLD);
}

void ReadMPU6050(void) {
  MPU6050_Angle angle;

  MPU6050_GetAngle(&angle);
  roll = angle.Roll;
  pitch = angle.Pitch;

  stability_state = MPU6050_GetStabilityState();
}

void RefreshDisplay(void) {
  uint8_t need_refresh = 0;

  if (windows != last_windows) {
    last_windows = windows;
    last_weight = -1.0f;
    last_speed = -1.0f;
    last_dist_left = 0xFFFF;
    last_dist_right = 0xFFFF;
    last_roll = -100.0f;
    last_pitch = -100.0f;
    last_mpu6050_online = 0xFF;
    need_refresh = 1;
  }

  switch (windows) {
  case 1:
    if (need_refresh) {
      OLED_ShowString(2, 10, "AUTO");
      OLED_ShowString(4, 10, storage_full ? "FULL!" : "ONLINE");
    }
    if ((uint32_t)(weight * 100) != (uint32_t)(last_weight * 100)) {
      uint32_t w_int = (uint32_t)weight;
      uint32_t w_dec = (uint32_t)(weight * 100) % 100;
      OLED_ShowNum(3, 10, w_int, 2);
      OLED_ShowChar(3, 12, '.');
      OLED_ShowNum(3, 13, w_dec / 10, 1);
      OLED_ShowNum(3, 14, w_dec % 10, 1);
      OLED_ShowString(3, 15, "kg");
      last_weight = weight;
    }
    break;

  case 2:
    if (need_refresh) {
      OLED_ShowCN(1, 1, 17, 1);
      OLED_ShowCN(1, 2, 18, 1);
      OLED_ShowCN(1, 3, 19, 1);
      OLED_ShowCN(1, 4, 20, 1);
      OLED_ShowCN(1, 5, 21, 1);
      OLED_ShowCN(1, 6, 22, 1);
      OLED_ShowChar(1, 13, '2');

      OLED_ShowCN(2, 1, 27, 1);
      OLED_ShowCN(2, 2, 28, 1);
      OLED_ShowCN(2, 3, 29, 1);
      OLED_ShowCN(2, 4, 30, 1);
      OLED_ShowChar(2, 9, ':');

      OLED_ShowCN(3, 1, 31, 1);
      OLED_ShowCN(3, 2, 32, 1);
      OLED_ShowCN(3, 3, 33, 1);
      OLED_ShowCN(3, 4, 34, 1);
      OLED_ShowChar(3, 9, ':');
    }
    if ((int32_t)(roll * 10) != (int32_t)(last_roll * 10) ||
        (int32_t)(pitch * 10) != (int32_t)(last_pitch * 10) || need_refresh) {
      OLED_ShowSignedNum(2, 10, (int32_t)roll, 2);
      OLED_ShowSignedNum(2, 13, (int32_t)pitch, 2);
      last_roll = roll;
      last_pitch = pitch;
    }
    if (mpu6050_online != last_mpu6050_online || need_refresh) {
      OLED_ShowString(4, 1, "MPU:");
      if (mpu6050_online) {
        OLED_ShowString(4, 5, "OK ");
      } else {
        OLED_ShowHexNum(4, 5, MPU6050_GetWhoAmI(), 2);
      }
      last_mpu6050_online = mpu6050_online;
    }
    break;

  case 3:
    if ((uint32_t)(speed * 100) != (uint32_t)(last_speed * 100) ||
        need_refresh) {
      uint32_t s_int = (uint32_t)speed;
      uint32_t s_dec = (uint32_t)(speed * 100) % 100;
      OLED_ShowNum(2, 10, s_int, 1);
      OLED_ShowChar(2, 11, '.');
      OLED_ShowNum(2, 12, s_dec / 10, 1);
      OLED_ShowNum(2, 13, s_dec % 10, 1);
      OLED_ShowString(2, 14, "m/s");
      last_speed = speed;
    }
    if (need_refresh) {
      OLED_ShowString(3, 10, storage_full ? "FULL!" : "OK");
      OLED_ShowString(4, 1, "SAW:");
      OLED_ShowString(4, 5, saw_running ? "ON " : "OFF");
      OLED_ShowString(4, 9, "CV:");
      OLED_ShowString(4, 13, conveyor_running ? "ON " : "OFF");
    }
    break;

  case 4:
    if (need_refresh) {
      OLED_ShowString(2, 10, storage_full ? "FULL!" : "ESTAB");
    }
    if (dist_left != last_dist_left || need_refresh) {
      OLED_ShowNum(3, 3, dist_left, 3);
      last_dist_left = dist_left;
    }
    if (dist_right != last_dist_right || need_refresh) {
      OLED_ShowNum(4, 3, dist_right, 3);
      last_dist_right = dist_right;
    }
    break;
  }

  if (stability_state != last_stability_state || need_refresh) {
    switch (stability_state) {
    case STABILITY_NORMAL:
      OLED_ShowString(4, 10, "SAFE ");
      break;
    case STABILITY_WARNING:
      OLED_ShowString(4, 10, "WARN!");
      break;
    case STABILITY_DANGER:
      OLED_ShowString(4, 10, "DANGER");
      break;
    }
    last_stability_state = stability_state;
  }
}

void DifferentialCompensation(float tilt_angle, uint8_t tilt_axis) {
  int8_t left_comp = 0;
  int8_t right_comp = 0;

  if (tilt_axis == 0) {
    if (tilt_angle > MPU6050_TILT_THRESHOLD_1) {
      right_comp = MPU6050_TILT_COMPENSATION;
      left_comp = -MPU6050_TILT_COMPENSATION / 2;
    } else if (tilt_angle < -MPU6050_TILT_THRESHOLD_1) {
      left_comp = MPU6050_TILT_COMPENSATION;
      right_comp = -MPU6050_TILT_COMPENSATION / 2;
    }
  } else {
    if (tilt_angle > MPU6050_TILT_THRESHOLD_1) {
      right_comp = MPU6050_TILT_COMPENSATION / 2;
      left_comp = MPU6050_TILT_COMPENSATION / 2;
    } else if (tilt_angle < -MPU6050_TILT_THRESHOLD_1) {
      right_comp = -MPU6050_TILT_COMPENSATION / 2;
      left_comp = -MPU6050_TILT_COMPENSATION / 2;
    }
  }

  if (left_comp != 0 || right_comp != 0) {
    uint8_t base_speed = user_motor_speed;
    int8_t new_left_speed = base_speed + left_comp;
    int8_t new_right_speed = base_speed + right_comp;

    if (new_left_speed < 10) new_left_speed = 10;
    if (new_left_speed > 100) new_left_speed = 100;
    if (new_right_speed < 10) new_right_speed = 10;
    if (new_right_speed > 100) new_right_speed = 100;

    Robot_SetSpeed((uint8_t)((new_left_speed + new_right_speed) / 2));
  } else {
    // 无需补偿时恢复用户设定的速度
    Robot_SetSpeed(user_motor_speed);
  }
}

void EmergencyBrake(void) {
  Robot_Move(ROBOT_STOP);
  SawMotor_Stop();
  ConveyorMotor_Stop();
  saw_running = 0;
  conveyor_running = 0;
}

int main(void) {
  // 配置独立看门狗 IWDG: LSI 40kHz / 64 = 625Hz, 2500 / 625 = 4秒超时
  IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
  IWDG_SetPrescaler(IWDG_Prescaler_64);
  IWDG_SetReload(2500);
  IWDG_ReloadCounter();
  IWDG_Enable();

  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2); // 系统级配置，只调一次
  OLED_Init();
  Key_Init();
  ESP8266_Init();
  L298N_Init();
  L298N_Cutter_Init();
  Sensor_Init();
  HX711_Init();
  HCSR04_Init();
  Buzzer_Init();
  Window_Show(windows);
  Robot_Move(GO_FORWARD);

  mpu6050_online = 0;
  {
    MPU6050_Status status = MPU6050_Init();
    if (status == MPU6050_OK) {
      mpu6050_online = 1;
      Buzzer_On(); Delay_ms(150); Buzzer_Off();
    } else {
      Buzzer_On(); Delay_ms(100); Buzzer_Off();
      Delay_ms(100);
      Buzzer_On(); Delay_ms(100); Buzzer_Off();
    }
  }

  uint32_t left_no_signal_ms = 0;
  uint32_t right_no_signal_ms = 0;

  uint8_t recv_buf[129]; // 多留1字节给 '\0' 终止符，防止满128字节时越界
  uint16_t recv_len;

  while (1) {
    IWDG_ReloadCounter(); // 喂狗
    // ==== TURN_180 非阻塞状态机 ====
    if (turning_180) {
      if (GetTick() - turn_start_tick >= TURN_180_DURATION_MS) {
        Robot_Move(GO_FORWARD);  // 转弯完成，恢复前进
        turning_180 = 0;
      }
      // 转弯期间仍进行传感器读取和安全检测，但跳过障碍物避让逻辑
    }

    // ==== 按键去抖处理（主循环中安全执行） ====
    if (key1_event) {
      Delay_ms(20);
      if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_1) == 0) {
        Window_Next();
      }
      key1_event = 0;
    }
    if (key2_event) {
      Delay_ms(20);
      if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_0) == 0) {
        Window_Previous();
      }
      key2_event = 0;
    }

    RefreshDisplay();

    ReadSensors();

    if (!mpu6050_online) {
      static uint32_t mpu_retry_tick = 0;
      if (GetTick() - mpu_retry_tick > 2000) {
        MPU6050_Status status = MPU6050_Init();
        if (status == MPU6050_OK) {
          mpu6050_online = 1;
        }
        mpu_retry_tick = GetTick();
      }
    }

    if (mpu6050_online) {
      ReadMPU6050();

      if (stability_state == STABILITY_DANGER) {
        EmergencyBrake();
      } else if (stability_state == STABILITY_WARNING) {
        if (fabs(roll) > fabs(pitch)) {
          DifferentialCompensation(roll, 0);
        } else {
          DifferentialCompensation(pitch, 1);
        }
      }
    }

    /* 蜂鸣器报警：姿态异常 或 粮仓已满 */
    {
      uint8_t buzz_level = 0;

      if (mpu6050_online) {
        if (stability_state == STABILITY_DANGER) {
          buzz_level = 2;
        } else if (stability_state == STABILITY_WARNING) {
          buzz_level = 1;
        }
      }

      if (storage_full && buzz_level < 2) {
        buzz_level = 1;
      }

      Buzzer_AlarmTick(buzz_level);
    }

    // 转弯期间跳过障碍物避让（避免干扰正在进行的转向）
    if (!turning_180) {
      dist_left = HCSR04_GetDistance(HCSR04_LEFT);
      dist_right = HCSR04_GetDistance(HCSR04_RIGHT);

      if (dist_left > 0 && dist_left <= HCSR04_NO_OBJECT_THRESHOLD) {
        left_no_signal_ms = 0;
      } else {
        left_no_signal_ms += 10;
      }

      if (dist_right > 0 && dist_right <= HCSR04_NO_OBJECT_THRESHOLD) {
        right_no_signal_ms = 0;
      } else {
        right_no_signal_ms += 10;
      }

      if (left_no_signal_ms >= HCSR04_EDGE_CONFIRM_MS &&
          right_no_signal_ms >= HCSR04_EDGE_CONFIRM_MS) {
        Robot_Move(ROBOT_STOP);
        left_no_signal_ms = 0;
        right_no_signal_ms = 0;
      } else if (left_no_signal_ms >= HCSR04_EDGE_CONFIRM_MS ||
                 right_no_signal_ms >= HCSR04_EDGE_CONFIRM_MS) {
        // 非阻塞 TURN_180：启动转弯，主循环顶部检查超时
        Robot_Move(TURN_180);
        turning_180 = 1;
        turn_start_tick = GetTick();
        left_no_signal_ms = 0;
        right_no_signal_ms = 0;
      }
    }

    // ===== 定时上报传感器数据到服务器（每500ms） =====
    {
      static uint32_t last_send_tick = 0;
      if (GetTick() - last_send_tick >= 500) {
        char json[256];
        uint8_t motor_stat = (saw_running || conveyor_running) ? 1 : 0;
        sprintf(json,
          "{\"weight\":%.2f,\"speed\":%.2f,\"motor_status\":%d,"
          "\"battery\":100,\"light\":0,\"storage_full\":%s,"
          "\"device_id\":\"STM32_001\"}",
          (double)weight, (double)speed, motor_stat,
          storage_full ? "true" : "false");
        ESP8266_SendFrame(json);
        last_send_tick = GetTick();
      }
    }

    recv_len = ESP8266_GetReceivedData(recv_buf, sizeof(recv_buf));
    if (recv_len > 0) {
      recv_buf[recv_len] = '\0';

      char *saveptr;
      char *token = strtok_r((char *)recv_buf, "\r\n", &saveptr);
      while (token != NULL) {
        char *cmd = strstr(token, "CMD:");
        if (cmd == token) {
          if (strcmp(cmd, "CMD:START") == 0) {
            SawMotor_Start(SAW_DEFAULT_SPEED);
            saw_running = 1;
            ConveyorMotor_Start(CONVEYOR_DEFAULT_SPEED);
            conveyor_running = 1;
            Robot_Move(GO_FORWARD);
          } else if (strncmp(cmd, "CMD:SPEED:", 10) == 0) {
            int spd = atoi(cmd + 10);
            if (spd >= 10 && spd <= 100) {
              user_motor_speed = (uint8_t)spd;
              Robot_SetSpeed((uint8_t)spd);
              Robot_Move(GO_FORWARD);
            }
          } else if (strcmp(cmd, "CMD:STOP") == 0) {
            Robot_Move(ROBOT_STOP);
          } else if (strcmp(cmd, "CMD:FORWARD") == 0) {
            Robot_Move(GO_FORWARD);
          } else if (strcmp(cmd, "CMD:BACKWARD") == 0) {
            Robot_Move(GO_BACKWARD);
          } else if (strcmp(cmd, "CMD:LEFT") == 0) {
            Robot_Move(TURN_LEFT);
          } else if (strcmp(cmd, "CMD:RIGHT") == 0) {
            Robot_Move(TURN_RIGHT);
          } else if (strcmp(cmd, "CMD:SAW_ON") == 0) {
            SawMotor_Start(SAW_DEFAULT_SPEED);
            saw_running = 1;
          } else if (strcmp(cmd, "CMD:SAW_OFF") == 0) {
            SawMotor_Stop();
            saw_running = 0;
          } else if (strcmp(cmd, "CMD:CONVEYOR_ON") == 0) {
            ConveyorMotor_Start(CONVEYOR_DEFAULT_SPEED);
            conveyor_running = 1;
          } else if (strcmp(cmd, "CMD:CONVEYOR_OFF") == 0) {
            ConveyorMotor_Stop();
            conveyor_running = 0;
          }
        }
        token = strtok_r(NULL, "\r\n", &saveptr);
      }
    }

    Delay_ms(10);
  }
}
