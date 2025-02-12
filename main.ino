#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
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

// PWM変換関数
float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// 各軸のタスク
void servoTask(void *pvParameters) {
    int channel = (int)pvParameters;
    bool isMoving = false;
    float velocity = 0.0;  // 現在の速度
    float maxVelocity = 5.0;  // 最大速度（角度変化量）
    float acceleration = 0.5;  // 加速度（角度増分）
    float decelerationPoint = 10.0; // 減速開始する距離（目標角度との差）

    while (true) {
        float angleDiff = abs(target_angles[channel] - current_angles[channel]);  // 目標との差分

        if (angleDiff > 0.1) { // 目標角度に達していない場合
            if (isMoving == false){
              // 立ち上がり 移動距離が減速開始距離より小さい場合、移動速度を最大から計算する
              if (angleDiff <= decelerationPoint){
                velocity = maxVelocity;
              }
            }
            isMoving = true;

            // **加速フェーズ**
            if (velocity < maxVelocity && angleDiff > decelerationPoint) {
                velocity += acceleration;  // 加速
                if (velocity > maxVelocity) velocity = maxVelocity; // 最大速度制限
            }

            // **減速フェーズ**
            if (angleDiff < decelerationPoint) {
                velocity -= acceleration;  // 減速
                if (velocity < angleDiff) velocity = angleDiff; // 停止直前は最小速度
            }

            // **移動**
            if ( angleDiff < velocity ){
              velocity=angleDiff;
            }
            if (current_angles[channel] < target_angles[channel]) {
                current_angles[channel] += velocity;
            } else {
                current_angles[channel] -= velocity;
            }

            // **PWMに変換して送信**
            int pwm_value = (int)mapFloat(current_angles[channel], 0.0, 180.0, (float)SERVO_MIN, (float)SERVO_MAX);
            PcaCommand command = {channel, pwm_value};
            xQueueSend(pcaQueue, &command, portMAX_DELAY);

            // **速度に応じたディレイ（台形加減速）**
            int iMTDelay = (int)(50 / velocity);
            vTaskDelay(pdMS_TO_TICKS(iMTDelay)); 

        } else { // **目標角度に到達**
            if (isMoving) {
                //Serial.printf("DONE %d\n", channel);
                PcaCommand command = {channel, -1};
                xQueueSend(pcaQueue, &command, portMAX_DELAY);
                isMoving = false;
                velocity = 0.0; // 停止時に速度リセット
            }
            vTaskDelay(pdMS_TO_TICKS(50)); // CPU負荷を抑えるための遅延
        }
    }
}

// PCA9685の書き込みタスク
void pcaTask(void *pvParameters) {
    PcaCommand command;
    while (xQueueReceive(pcaQueue, &command, portMAX_DELAY) == pdPASS) {
        if ( command.pwm_value == -1 ){
          Serial.printf("DONE %d\n", command.channel);
        }else{
          pwm.setPWM(command.channel, 0, command.pwm_value);
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
                    Serial.print(current_angles[i]);
                    if (i < 5) Serial.print(",");
                }
                Serial.println();
            } else {
                int channel, angle;
                if (sscanf(commandString.c_str(), "MOVE %d %d", &channel, &angle) != 2) {
                    Serial.println("ERROR: Invalid command format.");
                    continue;
                }
                if (channel >= 0 && channel < 6 && angle >= 0 && angle <= 180) {
                    if(target_angles[channel] == angle){
                      Serial.printf("DONE %d\n", channel);
                    }else{
                      target_angles[channel] = angle;
                    }
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
    Serial.println("INFO:Homing complete!");

    
    pcaQueue = xQueueCreate(50, sizeof(PcaCommand));

    for (int i = 0; i < 6; i++) {
        xTaskCreatePinnedToCore(servoTask, "ServoTask", 4096, (void *)i, 1, NULL, 1);
    }
    xTaskCreatePinnedToCore(pcaTask, "PCATask", 4096, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(serialTask, "SerialTask", 4096, NULL, 1, NULL, 1);
}

void loop() {
    vTaskDelay(portMAX_DELAY);
}
