#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// communication
const char* TARGET_NAME = "ARTEMIS_ARM"; 
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"

// ble5
BLEClient* pClient  = NULL;
BLERemoteCharacteristic* pRemoteCharacteristic;
bool btConnected = false;
bool doConnect = false;
BLEAdvertisedDevice* myDevice;
unsigned long lastBTSend = 0;
#define BT_SEND_INTERVAL 80

// oled
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define JOY1_X 35
#define JOY1_Y 34
#define JOY2_X 33
#define JOY2_Y 32

#define BTN1_PIN 5
#define BTN2_PIN 17
#define BTN3_PIN 18
#define SW_GRIP 15

// joystick param
#define JOY_RANGE 10 // range -10 to 10
#define DEADZONE 80

// grip mode
bool gripMode = false;
int G_value = 0;
bool lastGripSwitchState = HIGH;

#define EYE_MOVE_MAX 6

int CX, CY, CZ, CR;
int X=0, Y=0, Z=0;
int G_digital=0;
int btn1State=0, btn2State=0, btn3State=0;
int slideMode=0;

// smoothing variable for joystick
float X_f = 0, Y_f = 0, Z_f = 0;
float G_f = 0.0;
int T_value = 0;
int R_value = 0;
float T_f = 0;
float R_f = 0;

// ble5 callback
class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) { 
    btConnected = true; 
    Serial.println("BLE Connected!");
  }
  void onDisconnect(BLEClient* pclient) { 
    btConnected = false; 
    Serial.println("BLE Disconnected!");
  }
};

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    // find device with same name
    if (advertisedDevice.haveName() && advertisedDevice.getName() == TARGET_NAME) {
      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      Serial.println("Target device found!");
    }
  }
};

bool connectToServer() {
    Serial.println("Connecting to BLE Server...");
    
    pClient = BLEDevice::createClient();
    pClient->setClientCallbacks(new MyClientCallback());
    
    if(!pClient->connect(myDevice)) {
        Serial.println("Failed to connect");
        return false;
    }
    
    BLERemoteService* pRemoteService = pClient->getService(SERVICE_UUID);
    if (pRemoteService == nullptr) {
        Serial.println("Service not found");
        pClient->disconnect();
        return false;
    }

    pRemoteCharacteristic = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID_RX);
    if (pRemoteCharacteristic == nullptr) {
        Serial.println("Characteristic not found");
        pClient->disconnect();
        return false;
    }
    
    Serial.println("Connected to BLE Server!");
    return true;
}

// robot eye geometry
#define EYE_W 32
#define EYE_H 42
#define EYE_Y 10
#define EYE_GAP 14

#define LEFT_EYE_X  (64 - EYE_GAP/2 - EYE_W)
#define RIGHT_EYE_X (64 + EYE_GAP/2)

// timing
#define BLINK_TIME 80
#define IDLE_DURATION 20000
#define LOOK_TIME 2000
#define TRANSITION_TIME 400
#define SMILE_TIME 1400

// eye control state
enum EyeState {NORMAL, BLINK, LOOK, TRANSITION, SMILE};
EyeState eyeState=NORMAL;
unsigned long stateTimer=0;
unsigned long idleTimer=0;
bool lookRight=true;

// eye offset
int leftEyeOffsetX=0, leftEyeOffsetY=0;
int rightEyeOffsetX=0, rightEyeOffsetY=0;

// joystick calib
void calibrateJoysticks() {
  Serial.println("\n=== KALIBRASI JOYSTICK ===");
  Serial.println("LEPAS SEMUA JOYSTICK DARI TANGAN!");
  Serial.println("Kalibrasi dalam 3 detik...");
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(20, 20);
  display.print("KALIBRASI JOYSTICK");
  display.setCursor(30, 40);
  display.print("LEPAS JOYSTICK");
  display.display();
  delay(3000);
  
  int samples = 100;
  long sumX = 0, sumY = 0, sumZ = 0, sumR = 0;
  
  for(int i = 0; i < samples; i++) {
    sumX += analogRead(JOY1_X);
    sumY += analogRead(JOY1_Y);
    sumZ += analogRead(JOY2_Y);
    sumR += analogRead(JOY2_X);
    
    display.clearDisplay();
    display.setCursor(20, 10);
    display.print("KALIBRASI...");
    display.drawRect(14, 30, 100, 8, SSD1306_WHITE);
    display.fillRect(16, 32, i, 4, SSD1306_WHITE);
    display.setCursor(40, 45);
    display.printf("%d%%", (i*100)/samples);
    display.display();
    
    delay(5);
  }
  
  CX = sumX / samples;
  CY = sumY / samples;
  CZ = sumZ / samples;
  CR = sumR / samples;
  
  Serial.println("\nKalibrasi Selesai!");
  Serial.printf("Center Values:\n");
  Serial.printf("  X: %d\n", CX);
  Serial.printf("  Y: %d\n", CY);
  Serial.printf("  Z: %d\n", CZ);
  Serial.printf("  R: %d\n", CR);
  
  display.clearDisplay();
  display.setCursor(10, 10);
  display.print("KALIBRASI SELESAI");
  display.setCursor(10, 25);
  display.printf("X:%4d", CX);
  display.setCursor(70, 25);
  display.printf("Y:%4d", CY);
  display.setCursor(10, 40);
  display.printf("Z:%4d", CZ);
  display.setCursor(70, 40);
  display.printf("R:%4d", CR);
  display.display();
  delay(2000);
}

// manual calib
int readJoystickDirect(int pin, int center, bool invert = false) {
  int raw = analogRead(pin);
  int delta = raw - center;
  
  if(abs(delta) < DEADZONE) return 0;
  
  int maxPositive = 4095 - center;
  int maxNegative = -center;
  
  int value;
  if(delta > 0) {
    value = map(delta, DEADZONE, maxPositive, 1, JOY_RANGE);
  } else {
    value = map(delta, -DEADZONE, maxNegative, -1, -JOY_RANGE);
  }
  
  value = constrain(value, -JOY_RANGE, JOY_RANGE);
  return invert ? -value : value;
}

// update joystick for grip mode
void updateJoystickValues() {
  int jx = readJoystickDirect(JOY1_X, CX);
  int jy = readJoystickDirect(JOY1_Y, CY, true);
  int jz = readJoystickDirect(JOY2_Y, CZ);
  int jr = readJoystickDirect(JOY2_X, CR);
  
  // grip mode
  if (gripMode) {
    if (jz != 0) {
      G_f += -jz * 0.8;
      G_f = constrain(G_f, 0, 180);
      G_value = round(G_f);
    }
    
    R_f += (jr - R_f) * 0.4;
    if(jr == 0) R_f *= 0.85;
    if(abs(R_f) < 0.5) R_f = 0;
    R_value = round(R_f);
    R_value = constrain(R_value, -JOY_RANGE, JOY_RANGE);
    
    T_value = 0;
    T_f = 0;
    
    Z_f = 0;
    Z = 0;
    
  } else {
    // normal mode
    G_digital = 0;
    
    Z_f += (-jz - Z_f) * 0.4;
    if(jz == 0) Z_f *= 0.85;
    if(abs(Z_f) < 0.5) Z_f = 0;
    Z = round(Z_f);
    Z = constrain(Z, -JOY_RANGE, JOY_RANGE);
    
    T_f += (jr - T_f) * 0.4;
    if(jr == 0) T_f *= 0.85;
    if(abs(T_f) < 0.5) T_f = 0;
    T_value = round(T_f);
    T_value = constrain(T_value, -JOY_RANGE, JOY_RANGE);
    
    R_value = 0;
    R_f = 0;
    
    G_value = 0;
  }
  
  // exchange axis
  X_f += (jy - X_f) * 0.4;  // jy (dari Y-axis) menjadi X
  Y_f += (jx - Y_f) * 0.4;  // jx (dari X-axis) menjadi Y
  
  if(jy == 0) X_f *= 0.85;
  if(jx == 0) Y_f *= 0.85;
  
  if(abs(X_f) < 0.5) X_f = 0;
  if(abs(Y_f) < 0.5) Y_f = 0;
  
  X = round(X_f);
  Y = round(Y_f);
  X = constrain(X, -JOY_RANGE, JOY_RANGE);
  Y = constrain(Y, -JOY_RANGE, JOY_RANGE);
  
  leftEyeOffsetX = map(Y, -JOY_RANGE, JOY_RANGE, -EYE_MOVE_MAX, EYE_MOVE_MAX);  // Y untuk X offset
  leftEyeOffsetY = map(X, -JOY_RANGE, JOY_RANGE, EYE_MOVE_MAX, -EYE_MOVE_MAX);  // X untuk Y offset
  
  if (gripMode) {
    rightEyeOffsetX = map(R_value, -JOY_RANGE, JOY_RANGE, -EYE_MOVE_MAX, EYE_MOVE_MAX);
    rightEyeOffsetY = 0;
  } else {
    rightEyeOffsetX = map(T_value, -JOY_RANGE, JOY_RANGE, -EYE_MOVE_MAX, EYE_MOVE_MAX);
    rightEyeOffsetY = map(Z, -JOY_RANGE, JOY_RANGE, EYE_MOVE_MAX, -EYE_MOVE_MAX);
  }
  
  if(X != 0 || Y != 0 || Z != 0 || T_value != 0 || R_value != 0 || (gripMode && (jz != 0 || jr != 0))) {
    eyeState = NORMAL;
    idleTimer = millis();
  }
}

// loading screen
void showLoadingScreen(unsigned long durationMs = 2000) {
  const char* text = "Loading . . .";
  int textLen = strlen(text) * 6;
  int textX = (128 - textLen) / 2;
  int textY = 18;

  int barX = 14;
  int barY = 38;
  int barWidth = 100;
  int barHeight = 8;

  int blocks = 10;
  int blockW = barWidth / blocks;

  unsigned long start = millis();

  while (millis() - start < durationMs) {
    display.clearDisplay();

    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(textX, textY);
    display.print(text);

    display.drawRect(barX, barY, barWidth, barHeight, SSD1306_WHITE);

    float progress = float(millis() - start) / durationMs;
    int filledBlocks = progress * blocks;

    for (int i = 0; i < filledBlocks; i++) {
      display.fillRect(
        barX + 2 + i * blockW,
        barY + 2,
        blockW - 2,
        barHeight - 4,
        SSD1306_WHITE
      );
    }

    display.display();
    delay(60);
  }
}

// bluetooth icon
void drawBluetoothIcon() {
  int cx = 116;
  int cy = 11;
  int ovalW = 16;
  int ovalH = 22;
  int radius = 8;
  
  if (btConnected) {
    display.fillRoundRect(
      cx - ovalW/2,
      cy - ovalH/2,
      ovalW,
      ovalH,
      radius,
      SSD1306_WHITE
    );

    int h = 8;
    int w = 4;

    for (int i = -1; i <= 1; i++) {
      if (cx + i >= 0 && cx + i < SCREEN_WIDTH) {
        display.drawLine(cx + i, cy - h, cx + i, cy + h, SSD1306_BLACK);
      }
    }
    
    for (int i = -1; i <= 1; i++) {
      if (cx + i >= 0 && cx + i < SCREEN_WIDTH) {
        display.drawLine(cx + i, cy - h, cx + w + i, cy - h/2, SSD1306_BLACK);
        display.drawLine(cx + w + i, cy - h/2, cx + i, cy, SSD1306_BLACK);
      }
    }
    
    for (int i = -1; i <= 1; i++) {
      if (cx + i >= 0 && cx + i < SCREEN_WIDTH) {
        display.drawLine(cx + i, cy, cx + w + i, cy + h/2, SSD1306_BLACK);
        display.drawLine(cx + w + i, cy + h/2, cx + i, cy + h, SSD1306_BLACK);
      }
    }

    int arrowOffset = -5;
  int arrowSize = 5;
  
  // bluetooth construction ">"
  for (int i = -1; i <= 1; i++) {
    if (cx + arrowOffset + i >= 0 && cx + arrowOffset + i < SCREEN_WIDTH) {
      display.drawLine(
        cx + arrowOffset + i, cy - arrowSize,
        cx + arrowOffset + arrowSize + i, cy,
        SSD1306_BLACK
      );
      
      display.drawLine(
        cx + arrowOffset + i, cy + arrowSize,
        cx + arrowOffset + arrowSize + i, cy,
        SSD1306_BLACK
      );
    }
  }

  } else {
    // bluetooth disconnect icon
    display.drawRoundRect(
      cx - ovalW/2,
      cy - ovalH/2,
      ovalW,
      ovalH,
      radius,
      SSD1306_WHITE
    );
    
    // Cross mark
    display.drawLine(cx-4, cy-4, cx+4, cy+4, SSD1306_WHITE);
    display.drawLine(cx+4, cy-4, cx-4, cy+4, SSD1306_WHITE);
  }
}

// draw eye
void drawNormalEyes(){
  display.fillRoundRect(
    LEFT_EYE_X + leftEyeOffsetX,
    EYE_Y + leftEyeOffsetY,
    EYE_W, EYE_H, 10, SSD1306_WHITE);
  
  display.fillRoundRect(
    RIGHT_EYE_X + rightEyeOffsetX,
    EYE_Y + rightEyeOffsetY,
    EYE_W, EYE_H, 10, SSD1306_WHITE);
  
  drawBluetoothIcon();
}

void drawBlinkEyes(){
  int blinkHeight = 6;
  
  display.fillRoundRect(
    LEFT_EYE_X,
    EYE_Y + EYE_H/2 - blinkHeight/2,
    EYE_W, blinkHeight, 3, SSD1306_WHITE);

  display.fillRoundRect(
    RIGHT_EYE_X,
    EYE_Y + EYE_H/2 - blinkHeight/2,
    EYE_W, blinkHeight, 3, SSD1306_WHITE);
  
  drawBluetoothIcon();
}

void drawLookEyes(){
  int offset = lookRight ? 12 : -12;
  int squintH = 30;
  int squintY = EYE_Y + (EYE_H - squintH)/2;

  display.fillRoundRect(
    LEFT_EYE_X + offset,
    squintY,
    EYE_W, squintH, 8, SSD1306_WHITE);

  display.fillRoundRect(
    RIGHT_EYE_X + offset,
    squintY,
    EYE_W, squintH, 8, SSD1306_WHITE);
  
  drawBluetoothIcon();
}

void drawSmileEye(int cx,int cy){
  for(int x=-14;x<=14;x++){
    int y=(x*x)/30;
    for(int t=0;t<7;t++){
      display.drawPixel(cx+x, cy+y+t, SSD1306_WHITE);
      display.drawPixel(cx+x+1, cy+y+t, SSD1306_WHITE);
    }
  }
}

void drawSmileEyes(){
  int y = EYE_Y + EYE_H/2 - 10;
  drawSmileEye(LEFT_EYE_X + EYE_W/2, y);
  drawSmileEye(RIGHT_EYE_X + EYE_W/2, y);
  
  drawBluetoothIcon();
}

// eye state update
void updateEyes(){
  unsigned long now=millis();
  display.clearDisplay();

  switch(eyeState){
    case NORMAL:
      drawNormalEyes();
      if(now-stateTimer>3700){ 
        eyeState=BLINK; 
        stateTimer=now; 
      }
      if(now-idleTimer>IDLE_DURATION){ 
        eyeState=LOOK; 
        lookRight=true; 
        stateTimer=now; 
      }
      break;

    case BLINK:
      drawBlinkEyes();
      if(now-stateTimer>BLINK_TIME){ 
        eyeState=NORMAL; 
        stateTimer=now; 
      }
      break;

    case LOOK:
      drawLookEyes();
      if(now-stateTimer>LOOK_TIME/2) lookRight=false;
      if(now-stateTimer>LOOK_TIME){ 
        eyeState=TRANSITION; 
        stateTimer=now; 
      }
      break;

    case TRANSITION:
      drawNormalEyes();
      if(now-stateTimer>TRANSITION_TIME){ 
        eyeState=SMILE; 
        stateTimer=now; 
      }
      break;

    case SMILE:
      drawSmileEyes();
      if(now-stateTimer>SMILE_TIME){
        eyeState=NORMAL;
        idleTimer=now;
        stateTimer=now;
      }
      break;
  }
  
  // indicator mode display
  display.setTextSize(1);
  display.setCursor(2, 54);
  if (gripMode) {
    display.print("GRIP");
  } else {
    display.print("NORM");
  }
  
  display.display();
}

// ble5 connection function
void handleBluetoothConnect() {
  static bool lastBtn2 = HIGH;
  bool btn2Now = digitalRead(BTN2_PIN) == LOW;
  
  if (lastBtn2 == HIGH && btn2Now == LOW) {
    if (!btConnected) {
      // start scanning for BLE device
      display.clearDisplay();
      display.setCursor(20, 20);
      display.print("SCANNING ROBOT...");
      display.display();
      
      BLEScan* pBLEScan = BLEDevice::getScan();
      pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
      pBLEScan->setActiveScan(true);
      pBLEScan->start(5, false);
      
      display.clearDisplay();
      display.setCursor(10, 20);
      if (doConnect) {
        display.print("ROBOT FOUND!");
      } else {
        display.print("ROBOT NOT FOUND");
      }
      display.display();
      delay(1000);
      
    } else {
      if (pClient) {
        pClient->disconnect();
        btConnected = false;
        display.clearDisplay();
        display.setCursor(30, 20);
        display.print("DISCONNECTED");
        display.display();
        delay(500);
      }
    }
  }
  lastBtn2 = btn2Now;

  if (doConnect) {
    display.clearDisplay();
    display.setCursor(20, 20);
    display.print("CONNECTING...");
    display.display();
    
    if (connectToServer()) {
      Serial.println("Successfully connected to BLE server!");
      display.clearDisplay();
      display.setCursor(25, 20);
      display.print("CONNECTED!");
      display.display();
      delay(500);
    } else {
      Serial.println("Failed to connect to BLE server");
      display.clearDisplay();
      display.setCursor(20, 20);
      display.print("CONNECTION FAILED");
      display.display();
      delay(1000);
    }
    doConnect = false;
  }
}

// receive data ble5
void sendDataBluetooth(){
  if(!btConnected || !pRemoteCharacteristic) return;

  unsigned long now = millis();
  if(now - lastBTSend < BT_SEND_INTERVAL) return;
  lastBTSend = now;

  String packet;
  
  if (gripMode) {
    packet = "X:" + String(X) +     
             ",Y:" + String(Y) +     
             ",Z:0" +
             ",R:" + String(R_value) +
             ",G:" + String(G_value) +
             ",T:0" +
             ",B1:" + String(btn1State) + 
             ",B2:" + String(btn2State) + 
             ",B3:" + String(btn3State) + 
             ",M:1" +
             "\n";
  } else {
    packet = "X:" + String(X) +     
             ",Y:" + String(Y) +     
             ",Z:" + String(Z) +
             ",R:0" +
             ",G:" + String(G_digital) +
             ",T:" + String(T_value) +
             ",B1:" + String(btn1State) + 
             ",B2:" + String(btn2State) + 
             ",B3:" + String(btn3State) + 
             ",M:0" +
             "\n";
  }
  
  Serial.print("[TX] ");
  Serial.print(packet);
                  
  pRemoteCharacteristic->writeValue(packet.c_str(), packet.length());
}

// simple data oled display
void displayDataOnOLED(){
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  display.setCursor(45, 0);
  if (gripMode) {
    display.print("[GRIP]");
  } else {
    display.print("[NORMAL]");
  }

  drawBluetoothIcon();
  
  display.setCursor(0, 14);
  display.print("X:");
  if(X >= 0) display.print("+");
  display.printf("%2d", X);
  
  display.setCursor(64, 14);
  display.print("Y:");
  if(Y >= 0) display.print("+");
  display.printf("%2d", Y);
  
  if (gripMode) {
    display.setCursor(0, 26);
    display.print("G:");
    display.printf("%3d", G_value);
    
    display.setCursor(64, 26);
    display.print("R:");
    if(R_value >= 0) display.print("+");
    display.printf("%2d", R_value);
    
    display.setCursor(0, 38);
    display.print("G-BAR:");
    display.drawRect(40, 38, 86, 8, SSD1306_WHITE);
    int gBarWidth = map(G_value, 0, 180, 0, 84);
    display.fillRect(41, 39, gBarWidth, 6, SSD1306_WHITE);
    
    display.setCursor(0, 50);
    display.print("R-BAR:");
    display.drawRect(40, 50, 86, 8, SSD1306_WHITE);
    int rBarWidth = map(R_value + 10, 0, 20, 0, 84);
    display.fillRect(41, 51, rBarWidth, 6, SSD1306_WHITE);
    
  } else {
    display.setCursor(0, 26);
    display.print("Z:");
    if(Z >= 0) display.print("+");
    display.printf("%2d", Z);
    
    display.setCursor(64, 26);
    display.print("T:");
    if(T_value >= 0) display.print("+");
    display.printf("%2d", T_value);
    
    display.setCursor(0, 38);
    display.print("Z-BAR:");
    display.drawRect(40, 38, 86, 8, SSD1306_WHITE);
    int zBarWidth = map(Z + 10, 0, 20, 0, 84);
    display.fillRect(41, 39, zBarWidth, 6, SSD1306_WHITE);
    
    display.setCursor(0, 50);
    display.print("T-BAR:");
    display.drawRect(40, 50, 86, 8, SSD1306_WHITE);
    int tBarWidth = map(T_value + 10, 0, 20, 0, 84);
    display.fillRect(41, 51, tBarWidth, 6, SSD1306_WHITE);
  }
  
  display.display();
}

// switch handle mode grip
void handleGripModeSwitch() {
  bool gripSwitchNow = digitalRead(SW_GRIP);
  
  if (lastGripSwitchState == HIGH && gripSwitchNow == LOW) {
    delay(50);
    
    if (digitalRead(SW_GRIP) == LOW) {
      gripMode = !gripMode;
      
      if (gripMode) {
        G_value = 0;
        G_f = 0.0;
        R_value = 0;
        R_f = 0;
        Serial.println("Grip Mode: ACTIVATED");
      } else {
        G_digital = 0;
        T_value = 0;
        T_f = 0;
        Serial.println("Grip Mode: DEACTIVATED");
      }
    }
  }
  
  lastGripSwitchState = gripSwitchNow;
}

// setup and loop
void setup(){
  Serial.begin(115200);
  Serial.println("\n\n=== ARTEMIS REMOTE CONTROL ===");
  
  Wire.begin(21,22);
  if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("OLED allocation failed"));
    for(;;);
  }
  display.clearDisplay();
  display.display();

  showLoadingScreen(1500);

  BLEDevice::init("ARTEMIS_REMOTE");
  Serial.println("BLE Initialized");

  pinMode(BTN1_PIN, INPUT_PULLUP);
  pinMode(BTN2_PIN, INPUT_PULLUP);
  pinMode(BTN3_PIN, INPUT_PULLUP);
  pinMode(SW_GRIP, INPUT_PULLUP);

  calibrateJoysticks();

  idleTimer = millis();
  stateTimer = millis();

  lastGripSwitchState = digitalRead(SW_GRIP);
  if (lastGripSwitchState == LOW) {
    while(digitalRead(SW_GRIP) == LOW) {
      delay(10);
    }
    lastGripSwitchState = HIGH;
  }

  G_value = 0;
  G_f = 0.0;
  G_digital = 0;
  T_value = 0;
  T_f = 0;
  R_value = 0;
  R_f = 0;

  display.clearDisplay();
  display.setCursor(10, 10);
  display.print("ARTEMIS REMOTE");
  display.setCursor(5, 25);
  display.print("Mode: Normal");
  display.setCursor(10, 40);
  display.print("Press SW_GRIP");
  display.setCursor(15, 50);
  display.print("for Grip Mode");
  display.display();
  delay(1500);
  
  Serial.println("Setup completed!");
}

void loop(){
  static bool lastBtn1State = HIGH;
  bool btn1Now = digitalRead(BTN1_PIN);

  if(lastBtn1State == HIGH && btn1Now == LOW){
    delay(50);
    if(digitalRead(BTN1_PIN) == LOW) {
      slideMode = (slideMode + 1) % 2;
      Serial.printf("Display mode: %s\n", slideMode ? "DATA" : "EYES");
      
      display.clearDisplay();
      display.setCursor(30, 20);
      display.print(slideMode ? "DATA MODE" : "EYE MODE");
      display.display();
      delay(300);
    }
  }
  lastBtn1State = btn1Now;

  handleGripModeSwitch();

  btn1State = digitalRead(BTN1_PIN) == LOW;
  btn2State = digitalRead(BTN2_PIN) == LOW;
  btn3State = digitalRead(BTN3_PIN) == LOW;
  
  updateJoystickValues();

  switch(slideMode){
    case 0:
      updateEyes();
      break;
      
    case 1:
      displayDataOnOLED();
      break;
  }

  handleBluetoothConnect();

  sendDataBluetooth();

  delay(40);
}