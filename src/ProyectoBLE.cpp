#include "ProyectoBLE.h"
#include <sdkconfig.h>
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <Preferences.h>
#include <esp_arduino_version.h>
#include "Arduboy2.h"
#include "devices.hpp"
#include "led.hpp"

extern Arduboy2 arduboy;

namespace ProyectoBLEModule {

// Bluetooth maximum transmit power
#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C2) || defined(CONFIG_IDF_TARGET_ESP32S3)
#define BLE_MAX_TX_POWER ESP_PWR_LVL_P21  // ESP32C3 ESP32C2 ESP32S3
#elif defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32C6)
#define BLE_MAX_TX_POWER ESP_PWR_LVL_P20  // ESP32H2 ESP32C6
#else
#define BLE_MAX_TX_POWER ESP_PWR_LVL_P9   // Default
#endif

static BLEAdvertising *pAdvertising = nullptr;
static int currentMode = 0;
static Preferences preferences;

// Global parameters for the advertiser advertising cycles
static unsigned long lastChangeTime = 0;
static const unsigned long ADVERTISE_DURATION = 400; // 400 ms active transmission (optimized)
static const unsigned long SILENCE_DURATION = 150;   // 150 ms silence (optimized)
static bool isAdvertisingActive = false;
static esp_bd_addr_t current_mac = {0xFE, 0xED, 0xC0, 0xFF, 0xEE, 0x69};
static bool forceUpdate = true;
static int lastMode = -1;
static const char* lastBroadcastName = "Iniciando...";

#define RGB_LED_PIN 8

static void setLedState(bool state) {
  rgbLedWrite(RGB_LED_PIN, 0, 0, 0);
}

static void resetMode(){
  currentMode = 0;
  Serial.printf("Resetting mode to Android (0)\n");
  preferences.begin("my-app", false);
  preferences.putInt("mode", currentMode);
  preferences.end();
}

static void nextMode(){
  currentMode = (currentMode == 0) ? 1 : 0;
  Serial.printf("Updating mode to %s\n", (currentMode == 0) ? "Android" : "iPhone");
  preferences.begin("my-app", false);
  preferences.putInt("mode", currentMode);
  preferences.end();
}

static void setAdvertisementData(BLEAdvertisementData &oAdvertisementData, const AppleDevice& dev) {
  uint8_t packet[31];
  size_t packetLen;
  generatePacket(dev, packet, packetLen);
  lastBroadcastName = dev.name;
  Serial.printf("Broadcasting %s (Length: %d)...\n", dev.name, packetLen);

  #ifdef ESP_ARDUINO_VERSION_MAJOR
    #if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
        oAdvertisementData.addData(String((char*)packet, packetLen));
    #else
        oAdvertisementData.addData(std::string((char*)packet, packetLen));
    #endif
  #endif
}

static void setRandomDeviceData(BLEAdvertisementData &oAdvertisementData) {
  int idx = random(0, sizeof(ALL_DEVICES) / sizeof(ALL_DEVICES[0]));
  AppleDevice dev = ALL_DEVICES[idx];
  setAdvertisementData(oAdvertisementData, dev);
}

} // namespace ProyectoBLEModule

ProyectoBLEClass proyectoBLE;

void ProyectoBLEClass::init() {
  using namespace ProyectoBLEModule;
  Serial.println("Starting ESP32 BLE");

  preferences.begin("my-app", false);
  currentMode = preferences.getInt("mode", 0);
  if (currentMode != 0 && currentMode != 1) {
    currentMode = 0;
  }
  Serial.printf("Current Mode: %s\n", (currentMode == 0) ? "Android" : "iPhone");
  preferences.end();

  // Ensure RGB LED is completely OFF
  rgbLedWrite(RGB_LED_PIN, 0, 0, 0);

  BLEDevice::init("AirPods 69");
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, BLE_MAX_TX_POWER);

  BLEServer *pServer = BLEDevice::createServer();
  pAdvertising = pServer->getAdvertising();

  esp_bd_addr_t null_addr = {0xFE, 0xED, 0xC0, 0xFF, 0xEE, 0x69};
  #if defined(CONFIG_NIMBLE_ENABLED)
  BLEDevice::setOwnAddrType(BLE_OWN_ADDR_RANDOM);
  BLEDevice::setOwnAddr(null_addr);
  #else
  pAdvertising->setDeviceAddress(null_addr, BLE_ADDR_TYPE_RANDOM);
  #endif

  lastChangeTime = 0;
  forceUpdate = true;
  lastMode = -1;
  isAdvertisingActive = false;
  running = true;
}

void ProyectoBLEClass::update() {
  using namespace ProyectoBLEModule;
  if (!running || !pAdvertising) return;

  bool modeChanged = false;

  // Alternar entre modos Android e iPhone EXCLUSIVAMENTE con el Botón 3 físico (GPIO 7 -> A_BUTTON)
  if (arduboy.justPressed(A_BUTTON)) {
    nextMode();
    modeChanged = true;
  }

  if (Serial.available() > 0) {
    char c = Serial.read();
    if (c == 'r' || c == 'R') {
      resetMode();
      modeChanged = true;
    } else if (c == 'n' || c == 'N') {
      nextMode();
      modeChanged = true;
    } else if (c == '0') {
      currentMode = 0;
      Serial.println("Setting mode to Android (0) via Serial");
      preferences.begin("my-app", false);
      preferences.putInt("mode", currentMode);
      preferences.end();
      modeChanged = true;
    } else if (c == '1') {
      currentMode = 1;
      Serial.println("Setting mode to iPhone (1) via Serial");
      preferences.begin("my-app", false);
      preferences.putInt("mode", currentMode);
      preferences.end();
      modeChanged = true;
    }
  }

  // Mantener el LED RGB (GPIO 8) completamente apagado en todo momento
  rgbLedWrite(RGB_LED_PIN, 0, 0, 0);

  unsigned long now = millis();
  unsigned long elapsed = now - lastChangeTime;

  if (elapsed >= (ADVERTISE_DURATION + SILENCE_DURATION) || modeChanged || forceUpdate || currentMode != lastMode) {
    lastChangeTime = now;
    forceUpdate = false;
    lastMode = currentMode;

    pAdvertising->stop();

    for (int i = 0; i < 6; i++) {
      current_mac[i] = random(256);
      if (i == 0) {
        current_mac[i] |= 0xF0;
      }
    }

    BLEAdvertisementData oAdvertisementData = BLEAdvertisementData();
    AppleDevice selected_dev;

    const int totalDevices = sizeof(ALL_DEVICES) / sizeof(ALL_DEVICES[0]);
    int appleIndices[totalDevices];
    int appleCount = 0;
    int samsungIndices[totalDevices];
    int samsungCount = 0;

    for (int i = 0; i < totalDevices; i++) {
      if (ALL_DEVICES[i].type == SAMSUNG_BUDS || ALL_DEVICES[i].type == SAMSUNG_WATCH) {
        samsungIndices[samsungCount++] = i;
      } else {
        appleIndices[appleCount++] = i;
      }
    }

    if (currentMode == 0) {
      if (samsungCount > 0) {
        int randIdx = random(samsungCount);
        selected_dev = ALL_DEVICES[samsungIndices[randIdx]];
      } else {
        selected_dev = ALL_DEVICES[AIRPODS];
      }
    } else {
      if (appleCount > 0) {
        int randIdx = random(appleCount);
        selected_dev = ALL_DEVICES[appleIndices[randIdx]];
      } else {
        selected_dev = ALL_DEVICES[AIRPODS];
      }
    }

    setAdvertisementData(oAdvertisementData, selected_dev);

    if (selected_dev.type == SAMSUNG_BUDS || selected_dev.type == SAMSUNG_WATCH) {
#if defined(CONFIG_NIMBLE_ENABLED)
      pAdvertising->setAdvertisementType(BLE_GAP_CONN_MODE_UND);
#else
      pAdvertising->setAdvertisementType(ADV_TYPE_IND);
#endif
      oAdvertisementData.setFlags(0x06);
    } else {
      int adv_type_choice = random(2);
      if (adv_type_choice == 0) {
#if defined(CONFIG_NIMBLE_ENABLED)
        pAdvertising->setAdvertisementType(BLE_GAP_CONN_MODE_NON);
        pAdvertising->setScanResponse(true);
#else
        pAdvertising->setAdvertisementType(ADV_TYPE_SCAN_IND);
#endif
      } else {
#if defined(CONFIG_NIMBLE_ENABLED)
        pAdvertising->setAdvertisementType(BLE_GAP_CONN_MODE_NON);
        pAdvertising->setScanResponse(false);
#else
        pAdvertising->setAdvertisementType(ADV_TYPE_NONCONN_IND);
#endif
      }
    }

#if defined(CONFIG_NIMBLE_ENABLED)
    BLEDevice::setOwnAddr(current_mac);
#else
    pAdvertising->setDeviceAddress(current_mac, BLE_ADDR_TYPE_RANDOM);
#endif
    pAdvertising->setAdvertisementData(oAdvertisementData);

    pAdvertising->setMinInterval(0x20);
    pAdvertising->setMaxInterval(0x20);
    pAdvertising->setMinPreferred(0x20);
    pAdvertising->setMaxPreferred(0x20);

    int rand_val = random(100);
    if (rand_val < 70) {
        esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, BLE_MAX_TX_POWER);
    } else if (rand_val < 85) {
        esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, (esp_power_level_t)(BLE_MAX_TX_POWER - 1));
    } else if (rand_val < 95) {
        esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, (esp_power_level_t)(BLE_MAX_TX_POWER - 2));
    } else {
        esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, (esp_power_level_t)(BLE_MAX_TX_POWER - 3));
    }

    pAdvertising->start();
    isAdvertisingActive = true;
  }
  else if (elapsed >= ADVERTISE_DURATION && isAdvertisingActive) {
    pAdvertising->stop();
    isAdvertisingActive = false;
  }
}

void ProyectoBLEClass::draw() {
  using namespace ProyectoBLEModule;
  // Header
  arduboy.fillRect(0, 0, 128, 9, WHITE);
  arduboy.setTextColor(BLACK);
  arduboy.setCursor(20, 1);
  arduboy.print("PROYECTO BLE");
  arduboy.setTextColor(WHITE);

  // Status Frame
  arduboy.drawRect(4, 12, 120, 42, WHITE);

  // Current Target Mode
  arduboy.setCursor(10, 16);
  arduboy.print("MODO: ");
  if (currentMode == 0) {
    arduboy.print("ANDROID / SAMSUNG");
  } else {
    arduboy.print("IPHONE / APPLE");
  }

  // Active Device
  arduboy.setCursor(10, 27);
  arduboy.print("DEV: ");
  arduboy.print(lastBroadcastName);

  // RF Activity & TX Status
  arduboy.setCursor(10, 38);
  arduboy.print("TX: ");
  if (isAdvertisingActive) {
    arduboy.print("TRANSMITIENDO [>>]");
  } else {
    arduboy.print("PAUSA CICLO  [--]");
  }

  // Footer Navigation Bar
  arduboy.drawFastHLine(0, 56, 128, WHITE);
  arduboy.setCursor(2, 57);
  arduboy.print("B3:Cambiar  B1+B4:Menu");
}

void ProyectoBLEClass::stop() {
  using namespace ProyectoBLEModule;
  if (pAdvertising) {
    pAdvertising->stop();
  }
  BLEDevice::deinit(false);
  pAdvertising = nullptr;
  rgbLedWrite(8, 0, 0, 0);
  running = false;
}
