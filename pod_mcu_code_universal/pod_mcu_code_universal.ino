#include "ICM_20948.h"
#include "esp_adc_cal.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include "esp_bt_device.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <WiFiManager.h>
#include <Wire.h>
#include "SparkFun_BNO080_Arduino_Library.h"
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>

bool isbno = true;
#define EEPROM_SIZE 1

unsigned long lastTime = 0;
unsigned long timerDelay = 1000 * 60 * 2;

String batt_level = "";
bool displayOn = true;
bool otaMode = false;
int writeCount = 0;
boolean start = false;
BLECharacteristic *pCharacteristic;

struct Quat {
  float x, y, z, w;
} quat;

char buff[256];
bool rtcIrq = false;
bool initial = 1;
bool otaStart = false;

uint8_t func_select = 0;
uint8_t omm = 99;
uint8_t xcolon = 0;
uint32_t targetTime = 0;
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

#define BLE_NAME "WebXCube"

BLEUUID SERVICE_UUID("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
BLEUUID CHARACTERISTIC_UUID("6E400002-B5A3-F393-E0A9-E50E24DCCA9E");
BLEAdvertising *pAdvertising;

BNO080 myIMU;
bool bnoAvailable = false;

ICM_20948_I2C myICM;
bool icmAvailable = false;

void setupWiFi() {
  WiFiManager wifiManager;
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setBreakAfterConfig(true);
  mac_address = WiFi.macAddress();
  Serial.println(mac_address);
  wifiManager.autoConnect(String("MM-" + mac_address).c_str());
}

void configModeCallback(WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  Serial.println(myWiFiManager->getConfigPortalSSID());
}

void setupOTA() {
  ArduinoOTA.setHostname(mac_address.c_str());
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else
      type = "filesystem";
    Serial.println("Start updating " + type);
    otaStart = true;
  }).onEnd([]() {
    Serial.println("\nEnd");
    delay(500);
  }).onProgress([](unsigned int progress, unsigned int total) {
    int percentage = (progress / (total / 100));
  }).onError([](ota_error_t error) {
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

class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pChar) {
    String value = pChar->getValue();
    Serial.print("Received Value: ");
    Serial.println(value.c_str());

    if (value == "start") {
      if (!start) {
        start = true;
      }
    } else if (value == "calibrate") {
      // Calibration code here
    } else if (value == "dispoff") {
      displayOn = false;
    } else if (value == "restart") {
      delay(3000);
      ESP.restart();
    } else if (value == "ota") {
      EEPROM.write(0, 1);
      EEPROM.commit();
      delay(2000);
      ESP.restart();
    } else {
      // Handle other BLE commands
    }
  }
};

class ServerCallbacks : public BLEServerCallbacks {
  void onDisconnect(BLEServer *server) {
    Serial.print("Disconnected");
    pAdvertising->start();
  }
};

void setupBLE() {
  BLEDevice::init(BLE_NAME);
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY
  );
  pCharacteristic->setCallbacks(new MyCallbacks());
  pCharacteristic->addDescriptor(new BLE2902());

  mac_address = BLEDevice::getAddress().toString().c_str();
  esp_ble_gap_set_device_name(("MM-" + mac_address).c_str());
  esp_bt_dev_set_device_name(("MM-" + mac_address).c_str());

  pService->start();
  pAdvertising = pServer->getAdvertising();
  pAdvertising->start();
}

void setupIMU() {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(400000);

  if (myIMU.begin(0x69, Wire)) {
    myIMU.enableRotationVector(50);
    Serial.println(F("BNO080 detected and rotation vector enabled"));
    bnoAvailable = true;
  } else {
    Serial.println(F("BNO080 not detected. Trying ICM-20948"));
    myICM.begin(Wire, 0x69);
    if (myICM.status == ICM_20948_Stat_Ok) {
      myICM.initializeDMP();
      myICM.enableDMPSensor(INV_ICM20948_SENSOR_ORIENTATION);
      myICM.setDMPODRrate(DMP_ODR_Reg_Quat9, 0);
      myICM.enableFIFO();
      myICM.enableDMP();
      myICM.resetDMP();
      myICM.resetFIFO();
      icmAvailable = true;
      Serial.println(F("ICM-20948 detected and DMP enabled"));
    } else {
      Serial.println(F("No IMU detected. Check connections."));
    }
  }
}

void readBNO() {
  if (myIMU.dataAvailable()) {
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

void readICM() {
  icm_20948_DMP_data_t data;
  myICM.readDMPdataFromFIFO(&data);

  if (myICM.status == ICM_20948_Stat_Ok || myICM.status == ICM_20948_Stat_FIFOMoreDataAvail) {
    if ((data.header & DMP_header_bitmap_Quat9) > 0) {
      double q1 = ((double)data.Quat9.Data.Q1) / 1073741824.0;
      double q2 = ((double)data.Quat9.Data.Q2) / 1073741824.0;
      double q3 = ((double)data.Quat9.Data.Q3) / 1073741824.0;
      double q0 = sqrt(1.0 - ((q1 * q1) + (q2 * q2) + (q3 * q3)));

      Serial.print(q0, 3);
      Serial.print(" ");
      Serial.print(q1, 3);
      Serial.print(" ");
      Serial.print(q2, 3);
      Serial.print(" ");
      Serial.print(q3, 3);
      Serial.print(" Accuracy:");
      Serial.println(data.Quat9.Data.Accuracy);
    }
  }

  if (myICM.status != ICM_20948_Stat_FIFOMoreDataAvail) {
    myICM.resetFIFO();
  }
}

void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);

  pinMode(TP_PWR_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(CHARGE_PIN, INPUT);
  digitalWrite(TP_PWR_PIN, HIGH);

  esp_adc_cal_characteristics_t *adc_chars = (esp_adc_cal_characteristics_t *)calloc(1, sizeof(esp_adc_cal_characteristics_t));
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, vref, adc_chars);

  setupWiFi();
  setupOTA();
  setupBLE();
  setupIMU();
}

void loop() {
  ArduinoOTA.handle();

  if (start) {
    if (bnoAvailable) {
      readBNO();
    } else if (icmAvailable) {
      readICM();
    }
  }
}
