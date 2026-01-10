#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"

String inputString = "";
bool stringComplete = false;

// variable data
int X = 0, Y = 0, Z = 0, T = 0, R = 0, G = 0;
int BTN_1 = 0, BTN_2 = 0, BTN_3 = 0;
int Mode = 0;

// variable ble5
BLEServer *pServer = NULL;
BLECharacteristic *pCharacteristicTX;
bool deviceConnected = false;
bool oldDeviceConnected = false;
BLEAdvertising *pAdvertising = NULL;  // save pointer to advertising

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("Device connected");
    }

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("Device disconnected");
    }
};

class MyCharacteristicCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();

      if (rxValue.length() > 0) {
        inputString = "";
        for (int i = 0; i < rxValue.length(); i++) {
          char c = rxValue[i];
          if (c == '\n') {
            stringComplete = true;
          } else {
            inputString += c;
          }
        }
      }
    }
};

void parseData(String data) {
  // raw data display
  Serial.print("RAW: ");
  Serial.println(data);

  X = Y = Z = T = R = G = 0;
  BTN_1 = BTN_2 = BTN_3 = 0;
  Mode = 0;
  
  // manual parsing
  int xIndex = data.indexOf("X:");
  if (xIndex != -1) {
    int comma = data.indexOf(',', xIndex);
    if (comma == -1) comma = data.length();
    String val = data.substring(xIndex + 2, comma);
    X = val.toInt();
  }
  
  int yIndex = data.indexOf("Y:");
  if (yIndex != -1) {
    int comma = data.indexOf(',', yIndex);
    if (comma == -1) comma = data.length();
    String val = data.substring(yIndex + 2, comma);
    Y = val.toInt();
  }
  
  int zIndex = data.indexOf("Z:");
  if (zIndex != -1) {
    int comma = data.indexOf(',', zIndex);
    if (comma == -1) comma = data.length();
    String val = data.substring(zIndex + 2, comma);
    Z = val.toInt();
  }

  int tIndex = data.indexOf("T:");
  if (tIndex != -1) {
    int comma = data.indexOf(',', tIndex);
    if (comma == -1) comma = data.length();
    String val = data.substring(tIndex + 2, comma);
    T = val.toInt();
  }

  int rIndex = data.indexOf("R:");
  if (rIndex != -1) {
    int comma = data.indexOf(',', rIndex);
    if (comma == -1) comma = data.length();
    String val = data.substring(rIndex + 2, comma);
    R = val.toInt();
  }

  int gIndex = data.indexOf("G:");
  if (gIndex != -1) {
    int comma = data.indexOf(',', gIndex);
    if (comma == -1) comma = data.length();
    String val = data.substring(gIndex + 2, comma);
    G = val.toInt();
  }

  int b1Index = data.indexOf("B1:");
  if (b1Index != -1) {
    int comma = data.indexOf(',', b1Index);
    if (comma == -1) comma = data.length();
    String val = data.substring(b1Index + 3, comma);
    BTN_1 = val.toInt();
  }

  int b2Index = data.indexOf("B2:");
  if (b2Index != -1) {
    int comma = data.indexOf(',', b2Index);
    if (comma == -1) comma = data.length();
    String val = data.substring(b2Index + 3, comma);
    BTN_2 = val.toInt();
  }

  int b3Index = data.indexOf("B3:");
  if (b3Index != -1) {
    int comma = data.indexOf(',', b3Index);
    if (comma == -1) comma = data.length();
    String val = data.substring(b3Index + 3, comma);
    BTN_3 = val.toInt();
  }

  int mIndex = data.indexOf("M:");
  if (mIndex != -1) {
    String val = data.substring(mIndex + 2);
    Mode = val.toInt();
  }

  Serial.print("X:"); Serial.print(X);
  Serial.print(" Y:"); Serial.print(Y);
  Serial.print(" Z:"); Serial.print(Z);
  Serial.print(" T:"); Serial.print(T);
  Serial.print(" R:"); Serial.print(R);
  Serial.print(" G:"); Serial.print(G);
  Serial.print(" B1:"); Serial.print(BTN_1);
  Serial.print(" B2:"); Serial.print(BTN_2);
  Serial.print(" B3:"); Serial.print(BTN_3);
  Serial.print(" Mode:");
  if (Mode == 1) Serial.println("GRIP");
  else Serial.println("NORMAL");
}

void setup() {
  Serial.begin(115200);
  Serial.println("=== ARTEMIS RECEIVER WITH AUTO-RECONNECT ===");
  Serial.println("X, Y, Z, T, R, G, BTN1, BTN2, BTN3");
  Serial.println("---------------------------------------------");

  BLEDevice::init("ARTEMIS_ARM");
  
  // ble server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  
  // ble services
  BLEService *pService = pServer->createService(SERVICE_UUID);
  
  pCharacteristicTX = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_RX,
                      BLECharacteristic::PROPERTY_WRITE
                    );
  pCharacteristicTX->setCallbacks(new MyCharacteristicCallbacks());
  
  pService->start();
  
  pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  
  pAdvertising->setMinPreferred(0x06); // help iphone connection
  pAdvertising->setMinPreferred(0x12); // help android connection

  pAdvertising->start();
  
  Serial.println("BLE Ready. Advertising started...");
  Serial.println("Waiting for remote connection...");
}

void loop() {
  if (stringComplete) {
    parseData(inputString);
    inputString = "";
    stringComplete = false;
  }

  if (!deviceConnected && oldDeviceConnected) {
    delay(500);

    pAdvertising->start();
    Serial.println("Advertising restarted. Waiting for reconnection...");
    
    oldDeviceConnected = deviceConnected;
  }

  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
  }
  
  delay(10);
}