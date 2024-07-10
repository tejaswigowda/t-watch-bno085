#include "esp_adc_cal.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include "esp_bt_device.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <WiFiManager.h>  //https://github.com/tzapu/WiFiManager

#include <Wire.h>

#include "SparkFun_BNO080_Arduino_Library.h" // Click here to get the library: http://librarymanager/All#SparkFun_BNO080
BNO080 myIMU;

String response;

#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>

// define the number of bytes you want to access
#define EEPROM_SIZE 1

unsigned long lastTime = 0;
unsigned long timerDelay = 1000 * 60 * 2; // Timer set to 2 minutes

String batt_level = "";
bool displayOn = true;

bool otaMode = false;

int writeCount = 0;

boolean start = false;

BLECharacteristic *pCharacteristic;

struct Quat {
  float x;
  float y;
  float z;
  float w;
} quat;
struct Euler {
  float x;
  float y;
  float z;
} euler;
char buff[256];
bool rtcIrq = false;
bool initial = 1;
bool otaStart = false;

uint8_t func_select = 0;
uint8_t omm = 99;
uint8_t xcolon = 0;
uint32_t targetTime = 0;  // for next 1 second timeout
uint32_t colour = 0;
int vref = 1100;

bool pressed = false;
uint32_t pressedTime = 0;
bool charge_indication = false;

uint8_t hh, mm, ss;
String mac_address;
int pacnum = 0;

#define TP_PIN_PIN 33
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22
#define IMU_INT_PIN 38
#define RTC_INT_PIN 34
#define BATT_ADC_PIN 35
#define VBUS_PIN 36
#define TP_PWR_PIN 25
#define LED_PIN 4
#define CHARGE_PIN 32

#define BLE_NAME "WebXCube"  //must match filters name in bluetoothterminal.js- navigator.bluetooth.requestDevice

BLEUUID SERVICE_UUID("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");  // UART service UUID
BLEUUID CHARACTERISTIC_UUID("6E400002-B5A3-F393-E0A9-E50E24DCCA9E");
BLEAdvertising *pAdvertising;

void setupWiFi() {
  WiFiManager wifiManager;
  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setBreakAfterConfig(true);  // Without this saveConfigCallback does not get fired
  mac_address = WiFi.macAddress();
  Serial.println(mac_address);
  wifiManager.autoConnect(String("MM-" + mac_address).c_str());
}

void configModeCallback(WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
}

void drawProgressBar(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h, uint8_t percentage, uint16_t frameColor, uint16_t barColor) {

}

void setupOTA() {

  // Hostname defaults to esp3232-[MAC]
  ArduinoOTA.setHostname(mac_address.c_str());

  ArduinoOTA.onStart([]() {
              String type;
              if (ArduinoOTA.getCommand() == U_FLASH)
                type = "sketch";
              else  // U_SPIFFS
                type = "filesystem";

              // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
              Serial.println("Start updating " + type);
              otaStart = true;
            })
    .onEnd([]() {
      Serial.println("\nEnd");
      delay(500);
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      // Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
      int percentage = (progress / (total / 100));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");

      delay(3000);
      otaStart = false;
      initial = 1;
      targetTime = millis() + 1000;
      omm = 99;
    });

  ArduinoOTA.begin();
}

String getVoltage() {
  uint16_t v = analogRead(BATT_ADC_PIN);
  float battery_voltage = ((float)v / 4095.0) * 2.0 * 3.3 * (vref / 1000.0);
  return String(battery_voltage) + "V";
}

String Bone = "N/A";
bool calibrated = false;

class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pChar) {
    String value = pChar->getValue();
    Serial.print("Received Value: ");
    Serial.println(value.c_str());

    if (value == "start") {
      if (!start) {
      }
    } else if (value == "calibrate") {
      start = true;
    } else if (value == "dispoff") {
      if (calibrated) {
        displayOn = false;
      }
    } else if (value == "restart") {
      delay(3000);
      ESP.restart();
    } else if (value == "ota") {
      EEPROM.write(0, 1);
      EEPROM.commit();
      delay(2000);
      ESP.restart();
    } else {
      Bone = String(value.c_str());
    }
  }
};

class ServerCallbacks : public BLEServerCallbacks {
  void onDisconnect(BLEServer *server) {
    Serial.print("Disconnected");
    pAdvertising->start();
  }
};

void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);

  pinMode(LED_PIN, OUTPUT);

  int otaState = EEPROM.read(0);
  Serial.println(otaState);

  if (otaState == 1) {
    EEPROM.write(0, 0);
    EEPROM.commit();
    otaMode = true;
    otaStart = true;
    setupWiFi();
    setupOTA();
    return;
  }

  BLEDevice::init(BLE_NAME);
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);

  pCharacteristic->setCallbacks(new MyCallbacks());

  pCharacteristic->addDescriptor(new BLE2902());

  mac_address = BLEDevice::getAddress().toString().c_str();

  esp_ble_gap_set_device_name(("MM-" + mac_address).c_str());
  esp_bt_dev_set_device_name(("MM-" + mac_address).c_str());

  pService->start();

  pAdvertising = pServer->getAdvertising();
  pAdvertising->start();
  
  delay(2000); // Wait for BNO to boot
  // Start i2c and BNO080
  Wire.flush();   // Reset I2C
  myIMU.begin(0x69, Wire); // Use BNO080 address 0x69

  Wire.begin(9, 8); // I2C pins setup

  if (!myIMU.begin()) {
    Serial.println("BNO080 not detected at default I2C address. Check your connections and the BNO080 datasheet.");
    while (1);
  }

  Wire.setClock(400000); //Increase I2C data rate to 400kHz

  myIMU.enableRotationVector(50); // Send data update every 50ms

  Serial.println(F("Rotation vector enabled"));
  Serial.println(F("Output in form i, j, k, real, accuracy"));

  xTaskCreatePinnedToCore(
    TaskBluetooth, "TaskBluetooth", 10000, NULL, 1, NULL, 0);

  xTaskCreatePinnedToCore(
    TaskReadBNO, "TaskReadBNO", 10000, NULL, 1, NULL, 1);
}

void loop() {
  // Nothing to do in the loop, all tasks are handled by FreeRTOS tasks.
}

void IMU_Show() {
  if (myIMU.dataAvailable() == true) {
    quat.x = myIMU.getQuatI();
    quat.y = myIMU.getQuatJ();
    quat.z = myIMU.getQuatK();
    quat.w = myIMU.getQuatReal();
    
    Serial.print("Quaternion: ");
    Serial.print(quat.x, 4);
    Serial.print(", ");
    Serial.print(quat.y, 4);
    Serial.print(", ");
    Serial.print(quat.z, 4);
    Serial.print(", ");
    Serial.println(quat.w, 4);
  }
}

void TaskBluetooth(void *pvParameters) {
  for (;;) {
    static uint32_t prev_ms_ble = millis();
    if (millis() > prev_ms_ble + 1000 / 40) {
      prev_ms_ble = millis();
      String url = mac_address + " " + String(quat.x, 4) + " " + String(quat.y, 4) + " " + String(quat.z, 4) + " " + String(quat.w, 4);
      pCharacteristic->setValue(url.c_str());
      pCharacteristic->notify();
    }
    vTaskDelay(1);
  }
}

void TaskReadBNO(void *pvParameters) {
  for (;;) {
    IMU_Show();
    vTaskDelay(50); // Update every 50ms
  }
}
