#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <algorithm> // std::max() を使うために必要
#include "freertos/queue.h"
#include "freertos/task.h"

// PCA9685のI2Cアドレス
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);

// サーボモーターのPWMパルス範囲（500us～2500us）
#define SERVO_MIN  125  // 500us
#define SERVO_MAX  512  // 2500us

// 各軸の現在の角度を記録
float current_angles[6] = {90.0, 90.0, 90.0, 90.0, 90.0, 0.0};

// FreeRTOSキューの定義
QueueHandle_t pcaQueue;
// PCA9685へのコマンド構造体
struct PcaCommand {
    int channel;
    int pwm_value;
};

// 各軸の目標角度
float target_angles[6] = {90.0, 90.0, 90.0, 90.0, 90.0, 0.0};
float maxMoveTime = 0.0; // SYNC MOVE の最大移動時間

// PWM変換関数
float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// 各軸のタスク
void servoTask(void *pvParameters) {
    int channel = (int)pvParameters;
    bool isMoving = false;
    float velocity = 0.0;
    float acceleration = 0.5;
    float maxVelocity = 7.0;

    while (true) {
        float angleDiff = abs(target_angles[channel] - current_angles[channel]);

        if (angleDiff > 0.1) {
            if (!isMoving) {
                if (maxMoveTime > 0) {
                    velocity = std::max(1.0f, maxMoveTime / (angleDiff + 1.0f)); // 修正
                } else {
                    velocity = maxVelocity;
                }
            }
            isMoving = true;

            if (current_angles[channel] < target_angles[channel]) {
                current_angles[channel] += velocity;
                if (current_angles[channel] > target_angles[channel]) {
                    current_angles[channel] = target_angles[channel];
                }
            } else {
                current_angles[channel] -= velocity;
                if (current_angles[channel] < target_angles[channel]) {
                    current_angles[channel] = target_angles[channel];
                }
            }

            int pwm_value = (int)mapFloat(current_angles[channel], 0.0, 180.0, (float)SERVO_MIN, (float)SERVO_MAX);
            PcaCommand command = {channel, pwm_value};
            xQueueSend(pcaQueue, &command, portMAX_DELAY);

            vTaskDelay(pdMS_TO_TICKS(10 / velocity));
        } else { 
            if (isMoving) {
                PcaCommand command = {channel, -1};
                xQueueSend(pcaQueue, &command, portMAX_DELAY);
                isMoving = false;
            }
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }
}

// PCA9685の書き込みタスク
void pcaTask(void *pvParameters) {
    PcaCommand command;
    bool syncDoneSent = false;

    while (true) {
        if (xQueueReceive(pcaQueue, &command, portMAX_DELAY) == pdPASS) {
            if (command.pwm_value == -1) {
                // SYNC MOVE の場合、すべてのサーボが完了したかをチェック
                bool allDone = true;
                for (int i = 0; i < 6; i++) {
                    if (abs(target_angles[i] - current_angles[i]) > 0.1) {
                        allDone = false;
                        break;
                    }
                }

                if (allDone && !syncDoneSent) {
                    Serial.print("DONE SYNC");
                    for (int i = 0; i < 6; i++) {
                        if (target_angles[i] != 90.0) {
                            Serial.printf(" %d", i);
                        }
                    }
                    Serial.println();
                    syncDoneSent = true;
                }
            } else {
                pwm.setPWM(command.channel, 0, command.pwm_value);
                syncDoneSent = false;
            }
        }
    }
}

// シリアル通信タスク
void serialTask(void *pvParameters) {
    String commandString;
    while (true) {
        if (Serial.available()) {
            commandString = Serial.readStringUntil('\n');
            commandString.trim();

            if (commandString == "GETPOS") {
                Serial.print("POS:");
                for (int i = 0; i < 6; i++) {
                    Serial.print(current_angles[i], 2);
                    if (i < 5) Serial.print(",");
                }
                Serial.println();
            }else if (commandString.startsWith("SYNC MOVE")) {
                int channel[6], count = 0;
                float target_angle[6];
                float maxAngleDiff = 0.0;
                bool allDone = true;

                // **キューをクリアして前回のコマンドをリセット**
                xQueueReset(pcaQueue);

                const char *cmd = commandString.c_str();
                char buffer[128];
                strncpy(buffer, cmd, sizeof(buffer) - 1);
                buffer[sizeof(buffer) - 1] = '\0';

                char *token = strtok(buffer, " ");
                if (token == NULL) return;
                token = strtok(NULL, " ");
                if (token == NULL) return;

                while (token != NULL && count < 6) {
                    int ch;
                    float angle;
                    if (sscanf(token, "%d", &ch) == 1) {
                        token = strtok(NULL, " ");
                        if (token == NULL || sscanf(token, "%f", &angle) != 1) {
                            Serial.println("ERROR: Invalid SYNC MOVE format");
                            return;
                        }

                        // **チャンネルが範囲外なら無視**
                        if (ch < 0 || ch >= 6) {
                            Serial.printf("ERROR: Invalid channel %d (valid range: 0-5)\n", ch);
                            return;
                        }

                        channel[count] = ch;
                        target_angle[count] = angle;

                        // **すでに目標角度と一致しているかチェック**
                        if (abs(target_angle[count] - current_angles[channel[count]]) > 0.1) {
                            allDone = false;
                        }
                        count++;
                    }
                    token = strtok(NULL, " ");
                }

                // **すべてのサーボがすでに目標角度なら即 `DONE SYNC` を送信**
                if (allDone) {
                    Serial.print("DONE SYNC");
                    for (int i = 0; i < count; i++) {
                        Serial.printf(" %d", channel[i]);
                    }
                    Serial.println();
                    //return;
                }else{

                  // **最大移動距離を計算**
                  for (int i = 0; i < count; i++) {
                      float diff = abs(target_angle[i] - current_angles[channel[i]]);
                      if (diff > maxAngleDiff) maxAngleDiff = diff;
                  }
  
                  maxMoveTime = maxAngleDiff / 6.0;  // **高速化対応**
                  
                  for (int i = 0; i < count; i++) {
                      target_angles[channel[i]] = target_angle[i];
                  }
  
                  Serial.print("SYNC MOVE START:");
                  for (int i = 0; i < count; i++) {
                      Serial.printf(" [%d -> %.2f]", channel[i], target_angle[i]);
                  }
                  Serial.println();
                }
            }

          else if (commandString.startsWith("MOVE")) {  
              int channel;
              float angle;
              
              // `MOVE <channel> <angle>` をパース
              if (sscanf(commandString.c_str(), "MOVE %d %f", &channel, &angle) != 2) {
                  Serial.println("ERROR: Invalid MOVE command format.");
                  return;
              }
          
              // チャンネルと角度が有効範囲内かチェック
              if (channel < 0 || channel >= 6 || angle < 0.0 || angle > 180.0) {
                  Serial.println("ERROR: Invalid channel or angle.");
                  return;
              }
          
              // **すでに目標角度と現在角度が同じなら即座に DONE を返す**
              if (target_angles[channel] == angle && current_angles[channel] == angle) {
                  Serial.printf("DONE %d\n", channel);
              } else {
                  // **目標角度を更新**
                  target_angles[channel] = angle;
          
                  // **MOVE の場合、maxMoveTime をリセットして最速移動**
                  maxMoveTime = 0;
          
                  Serial.printf("MOVE %d %.2f\n", channel, angle);
              }
          }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}


void setup() {
    Serial.begin(115200);
    Wire.begin();
    pwm.begin();
    pwm.setPWMFreq(50);

    // **原点復帰処理（仮）**
    int pwm_value = map(90, 0, 180, SERVO_MIN, SERVO_MAX);
    pwm.setPWM(1, 0, pwm_value);
    pwm.setPWM(2, 0, pwm_value);
    pwm.setPWM(3, 0, pwm_value);
    pwm.setPWM(4, 0, pwm_value);
    delay(300);
    pwm.setPWM(0, 0, pwm_value);

    pwm_value = map(0, 0, 180, SERVO_MIN, SERVO_MAX);
    pwm.setPWM(5, 0, pwm_value);
    
    Serial.println("INFO:Homing complete!");


    pcaQueue = xQueueCreate(200, sizeof(PcaCommand));

    for (int i = 0; i < 6; i++) {
        xTaskCreatePinnedToCore(servoTask, "ServoTask", 4096, (void *)i, 1, NULL, 1);
    }
    xTaskCreatePinnedToCore(pcaTask, "PCATask", 4096, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(serialTask, "SerialTask", 4096, NULL, 1, NULL, 1);

    Serial.println("INFO: PCA9685 Initialized with FreeRTOS");
}

void loop() {
    vTaskDelay(portMAX_DELAY);
}
