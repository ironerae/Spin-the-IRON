#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>

// ESP32-C3 MAC Address
uint8_t broadcastAddress[] = {0xE0, 0x72, 0xA1, 0x6F, 0xD8, 0xB8};

const int DRIVE_X_PIN = 33;
const int DRIVE_Y_PIN = 32;
const int WEAPON_PIN = 34;

const uint32_t SECRET_PASSWORD = 19842024;

typedef struct
{
  uint32_t password;
  int drive_x;
  int drive_y;
  int weapon_target;
} ControlData;

ControlData command;
esp_now_peer_info_t peerInfo;
unsigned long lastSendTime = 0;

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {}

void setup()
{
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  // THE HARDCORE WI-FI SLEEP KILLER
  esp_wifi_set_ps(WIFI_PS_NONE);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK)
    return;
  esp_now_register_send_cb(OnDataSent);

  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 1;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);
}

void loop()
{
  // adjust this if you feel the transmitter is having too much delay with the receiver. 50ms works well during my testing at home but I adjusted it to 120ms timer during the match due to some lags I experienced.
  if (millis() - lastSendTime >= 120)
  {
    lastSendTime = millis();

    int raw_drive_x = analogRead(DRIVE_X_PIN);
    int raw_drive_y = analogRead(DRIVE_Y_PIN);
    int raw_weapon = analogRead(WEAPON_PIN);

    // 1. DRIVE Y (Forward/Reverse)
    if (raw_drive_y < 1730)
    {
      command.drive_y = map(raw_drive_y, 1730, 0, 0, 255);
    }
    else if (raw_drive_y > 1930)
    {
      command.drive_y = map(raw_drive_y, 1930, 4095, 0, -255);
    }
    else
    {
      command.drive_y = 0;
    }

    // 2. DRIVE X (Steering Left/Right)
    if (raw_drive_x > 2050)
    {
      command.drive_x = map(raw_drive_x, 2050, 4095, 0, -255);
    }
    else if (raw_drive_x < 1810)
    {
      command.drive_x = map(raw_drive_x, 1810, 0, 0, 255);
    }
    else
    {
      command.drive_x = 0;
    }

    // 3. WEAPON (Spins CW only)
    if (raw_weapon < 1800)
    {
      command.weapon_target = map(raw_weapon, 1800, 0, 1000, 2000);
    }
    else
    {
      command.weapon_target = 1000;
    }

    command.password = SECRET_PASSWORD;

    esp_now_send(broadcastAddress, (uint8_t *)&command, sizeof(command));
  }
}