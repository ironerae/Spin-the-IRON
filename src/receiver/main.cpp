#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <ESP32Servo.h>

Servo weaponESC;

const int ESC_PIN = 5;
const int PWMA = 0;
const int AIN1 = 1;
const int AIN2 = 3;
const int PWMB = 4;
const int BIN1 = 6;
const int BIN2 = 10;

const uint32_t SECRET_PASSWORD = 19842024;

typedef struct
{
   uint32_t password;
   int drive_x;
   int drive_y;
   int weapon_target;
} ControlData;

volatile ControlData command;
volatile unsigned long last_packet_time = 0;
unsigned long last_motor_update = 0;

void setMotors(int leftSpeed, int rightSpeed)
{
   // Left Motor: Hard Brake or Drive
   if (leftSpeed == 0)
   {
      digitalWrite(AIN1, 0);
      digitalWrite(AIN2, 0);
      analogWrite(PWMA, 0);
   }
   else
   {
      digitalWrite(AIN1, (leftSpeed > 0) ? 1 : 0);
      digitalWrite(AIN2, (leftSpeed > 0) ? 0 : 1);
      analogWrite(PWMA, abs(leftSpeed));
   }

   // Right Motor: Hard Brake or Drive
   if (rightSpeed == 0)
   {
      digitalWrite(BIN1, 0);
      digitalWrite(BIN2, 0);
      analogWrite(PWMB, 0);
   }
   else
   {
      digitalWrite(BIN1, (rightSpeed > 0) ? 1 : 0);
      digitalWrite(BIN2, (rightSpeed > 0) ? 0 : 1);
      analogWrite(PWMB, abs(rightSpeed));
   }
}

void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
{
   // Quick size check to prevent crashes
   if (len == sizeof(ControlData))
   {
      // ZERO-COPY METHOD: Cast the raw incoming bytes directly to our struct format
      ControlData *incoming = (ControlData *)incomingData;

      // Validate the password
      if (incoming->password == SECRET_PASSWORD)
      {
         // Copy only the variables we need directly to the active command
         command.drive_x = incoming->drive_x;
         command.drive_y = incoming->drive_y;
         command.weapon_target = incoming->weapon_target;
         last_packet_time = millis();
      }
   }
}

void setup()
{
   Serial.begin(115200);

   pinMode(AIN1, OUTPUT);
   pinMode(AIN2, OUTPUT);
   pinMode(BIN1, OUTPUT);
   pinMode(BIN2, OUTPUT);
   pinMode(PWMA, OUTPUT);
   pinMode(PWMB, OUTPUT);
   setMotors(0, 0);

   weaponESC.setPeriodHertz(50);
   weaponESC.attach(ESC_PIN, 1000, 2000);
   weaponESC.writeMicroseconds(1000);

   WiFi.mode(WIFI_STA);

   // THE HARDCORE WI-FI SLEEP KILLER
   esp_wifi_set_ps(WIFI_PS_NONE);
   esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

   delay(3000);

   if (esp_now_init() != ESP_OK)
      return;
   esp_now_register_recv_cb(OnDataRecv);
}

void loop()
{
   // 1. Failsafe
   if (millis() - last_packet_time > 500)
   {
      command.drive_x = 0;
      command.drive_y = 0;
      command.weapon_target = 1000;
   }

   // 2. ONLY update hardware pins every 20ms (50Hz) to prevent timer crashes
   if (millis() - last_motor_update >= 10)
   {
      last_motor_update = millis();

      // Arcade Drive
      int left_speed = command.drive_y + command.drive_x;
      int right_speed = command.drive_y - command.drive_x;

      left_speed = constrain(left_speed, -255, 255);
      right_speed = constrain(right_speed, -255, 255);

      setMotors(left_speed, right_speed);

      // Instant Weapon Response
      weaponESC.writeMicroseconds(command.weapon_target);
   }
}