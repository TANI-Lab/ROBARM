#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include "freertos/queue.h"
#include "freertos/task.h"

// PCA9685のI2Cアドレス
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);

// サーボモーターのPWMパルス範囲（500us～2500us）
#define SERVO_MIN  125  // 500us
#define SERVO_MAX  625  // 2500us

// 各軸の現在の角度を記録
int current_angles[6] = {90, 90, 90, 90, 90, 0};

// FreeRTOSキューの定義
QueueHandle_t pcaQueue;

// PCA9685へのコマンド構造体
struct PcaCommand {
    int channel;
    int pwm_value;
};

// 各軸の目標角度を管理
int target_angles[6] = {90, 90, 90, 90, 90, 0};

// 各軸のタスク（移動完了時に "DONE" を送信）
void servoTask(void *pvParameters) {
    int channel = *(int *)pvParameters;
    bool isMoving = false;

    while (true) {
        if (current_angles[channel] != target_angles[channel]) {
            isMoving = true;

            if (current_angles[channel] < target_angles[channel]) {
                current_angles[channel]++;
            } else {
                current_angles[channel]--;
            }
            int pwm_value = map(current_angles[channel], 0, 180, SERVO_MIN, SERVO_MAX);
            PcaCommand command = {channel, pwm_value};
            xQueueSend(pcaQueue, &command, portMAX_DELAY);

            vTaskDelay(pdMS_TO_TICKS(10)); // 1度ごとの遅延
        } else {
            if (isMoving) {
                Serial.printf("DONE %d\n", channel);  // 移動完了を通知
                isMoving = false;
            }
            vTaskDelay(pdMS_TO_TICKS(50)); // 負荷低減
        }
    }
}

// PCA9685への書き込みタスク（キューで順番に処理）
void pcaTask(void *pvParameters) {
    PcaCommand command;
    while (true) {
        if (xQueueReceive(pcaQueue, &command, portMAX_DELAY) == pdPASS) {
            pwm.setPWM(command.channel, 0, command.pwm_value);
        }
    }
}

// **シリアル通信タスク（PCからコマンドを受信）**
void serialTask(void *pvParameters) {
    String commandString;
    while (true) {
        if (Serial.available()) {
            commandString = Serial.readStringUntil('\n');
            commandString.trim();

            if (commandString == "GETPOS") {
                // 現在の位置情報を送信
                Serial.print("POS:");
                for (int i = 0; i < 6; i++) {
                    Serial.print(current_angles[i]);
                    if (i < 5) Serial.print(",");
                }
                Serial.println();
            } else {
                int channel, angle;
                if (sscanf(commandString.c_str(), "MOVE %d %d", &channel, &angle) == 2) {
                    if (channel >= 0 && channel < 6 && angle >= 0 && angle <= 180) {
                        target_angles[channel] = angle;  // 目標角度を更新
                    } else {
                        Serial.println("ERROR: Invalid channel or angle");
                    }
                } else {
                    Serial.println("ERROR: Invalid command. Use: MOVE <channel> <angle> or GETPOS");
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));  // 10msごとにチェック
    }
}

void setup() {
    Serial.begin(115200);
    Wire.begin();
    pwm.begin();
    pwm.setPWMFreq(50);  // サーボのPWM周波数（50Hz）

    // **原点復帰処理（仮）**
    int pwm_value = map(90, 0, 180, SERVO_MIN, SERVO_MAX);
    pwm.setPWM(1, 0, pwm_value);
    pwm.setPWM(2, 0, pwm_value);
    pwm.setPWM(3, 0, pwm_value);
    pwm.setPWM(4, 0, pwm_value);
    delay(300);
    pwm.setPWM(0, 0, pwm_value);
    Serial.println("INFO:Homing complete!");

    // **FreeRTOSキューの作成（最大10個のコマンドを保存可能）**
    pcaQueue = xQueueCreate(10, sizeof(PcaCommand));

    // **PCA9685の書き込みタスク**
    xTaskCreatePinnedToCore(pcaTask, "PCA Task", 4096, NULL, 1, NULL, 0);

    // **各軸のタスクを作成**
    for (int i = 0; i < 6; i++) {
        int *channel = (int *)malloc(sizeof(int));
        *channel = i;
        xTaskCreatePinnedToCore(servoTask, "Servo Task", 4096, channel, 1, NULL, 1);
    }

    // **シリアル通信タスク**
    xTaskCreatePinnedToCore(serialTask, "Serial Task", 4096, NULL, 1, NULL, 1);

    Serial.println("INFO:PCA9685 Initialized with FreeRTOS");
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));  // メインループは何もしない
}
