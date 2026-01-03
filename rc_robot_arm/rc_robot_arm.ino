#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BluetoothSerial.h>

// oled
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// bluetooth
BluetoothSerial SerialBT;
#define BT_SEND_INTERVAL 80 // ms (keknya aman buat oled)
unsigned long lastBTSend = 0;

// bluetooth target
const char* ROBOT_BT_NAME = "ARTEMIS_ARM";
bool btConnected = false;

#define JOY1_X 35
#define JOY1_Y 34
#define JOY2_X 33
#define JOY2_Y 32

#define BTN1_PIN 5
#define BTN2_PIN 17
#define BTN3_PIN 18
#define SW_GRIP 15

// param
#define DEADZONE 120
#define EYE_MOVE_MAX 6
#define STEP 2
#define STEP_R 2

// range value
#define X_MIN -100
#define X_MAX 100
#define Y_MIN 30
#define Y_MAX 150
#define Z_MIN -50
#define Z_MAX 150
#define R_MIN -180
#define R_MAX 180

int CX, CY, CZ, CR;
int X=0, Y=50, Z=0, R=0;
int G=0;
int btn1State=0, btn2State=0, btn3State=0;
int slideMode=0;

// robot eyes geometry
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

// eyes behavior control
enum EyeState {NORMAL, BLINK, LOOK, TRANSITION, SMILE};
EyeState eyeState=NORMAL;
unsigned long stateTimer=0;
unsigned long idleTimer=0;
bool lookRight=true;

// eyes offset
int leftEyeOffsetX=0, leftEyeOffsetY=0;
int rightEyeOffsetX=0, rightEyeOffsetY=0;

// joystick
int readJoystick(int pin,int center,bool invert=false){
  int raw=analogRead(pin);
  int delta=raw-center;
  if(abs(delta)<DEADZONE) return 0;
  int val=map(delta,-2048,2047,-100,100);
  return invert?-val:val;
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

  // bluetooth construction
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
}

// draw eyes
void drawNormalEyes(){
  display.fillRoundRect(
    LEFT_EYE_X + leftEyeOffsetX,
    EYE_Y + leftEyeOffsetY,
    EYE_W, EYE_H, 10, SSD1306_WHITE);

  display.fillRoundRect(
    RIGHT_EYE_X + rightEyeOffsetX,
    EYE_Y + rightEyeOffsetY,
    EYE_W, EYE_H, 10, SSD1306_WHITE);
  
  if(btConnected && SerialBT.connected()) {
    drawBluetoothIcon();
  }
}

void drawBlinkEyes(){
  display.fillRoundRect(
    LEFT_EYE_X,
    EYE_Y + EYE_H/2,
    EYE_W, 6, 3, SSD1306_WHITE);

  display.fillRoundRect(
    RIGHT_EYE_X,
    EYE_Y + EYE_H/2,
    EYE_W, 6, 3, SSD1306_WHITE);
  
  if(btConnected && SerialBT.connected()) {
    drawBluetoothIcon();
  }
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
  
  if(btConnected && SerialBT.connected()) {
    drawBluetoothIcon();
  }
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
  
  if(btConnected && SerialBT.connected()) {
    drawBluetoothIcon();
  }
}

// eyes control update
void updateEyes(){
  unsigned long now=millis();
  display.clearDisplay();

  switch(eyeState){
    case NORMAL:
      drawNormalEyes();
      if(now-stateTimer>3700){ eyeState=BLINK; stateTimer=now; }
      if(now-idleTimer>IDLE_DURATION){ eyeState=LOOK; lookRight=true; stateTimer=now; }
      break;

    case BLINK:
      drawBlinkEyes();
      if(now-stateTimer>BLINK_TIME){ eyeState=NORMAL; stateTimer=now; }
      break;

    case LOOK:
      drawLookEyes();
      if(now-stateTimer>LOOK_TIME/2) lookRight=false;
      if(now-stateTimer>LOOK_TIME){ eyeState=TRANSITION; stateTimer=now; }
      break;

    case TRANSITION:
      drawNormalEyes();
      if(now-stateTimer>TRANSITION_TIME){ eyeState=SMILE; stateTimer=now; }
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
  display.display();
}

void handleBluetoothConnect() {
  static bool lastBtn2 = HIGH;
  static unsigned long lastPressTime = 0;
  #define DEBOUNCE_DELAY 200  // ms
  
  bool btn2Now = digitalRead(BTN2_PIN);
  unsigned long now = millis();

  if (lastBtn2 == HIGH && btn2Now == LOW && (now - lastPressTime > DEBOUNCE_DELAY)) {
    lastPressTime = now;
    
    if (!btConnected) {
      btConnected = SerialBT.connect(ROBOT_BT_NAME);
      if (btConnected) {
      }
    } else {
      SerialBT.disconnect();
      delay(100);
      btConnected = false;
    }
  }
  lastBtn2 = btn2Now;

  if (btConnected && !SerialBT.connected()) {
    btConnected = false;
  }
}

// bluetooth send data
void sendDataBluetooth(){
  if(!btConnected || !SerialBT.connected()) return;

  unsigned long now = millis();
  if(now - lastBTSend < BT_SEND_INTERVAL) return;
  lastBTSend = now;

  SerialBT.print("X:");  SerialBT.print(X);
  SerialBT.print(",Y:"); SerialBT.print(Y);
  SerialBT.print(",Z:"); SerialBT.print(Z);
  SerialBT.print(",R:"); SerialBT.print(R);
  SerialBT.print(",G:"); SerialBT.print(G);
  SerialBT.print(",B1:"); SerialBT.print(btn1State);
  SerialBT.print(",B2:"); SerialBT.print(btn2State);
  SerialBT.print(",B3:"); SerialBT.println(btn3State);
}

// joystick eyes
void updateEyeFromJoystick(){
  int lx = readJoystick(JOY1_X, CX);
  int ly = readJoystick(JOY1_Y, CY);
  int rx = readJoystick(JOY2_X, CR);
  int ry = readJoystick(JOY2_Y, CZ);

  leftEyeOffsetX  = map(lx,-100,100,-EYE_MOVE_MAX,EYE_MOVE_MAX);
  leftEyeOffsetY  = map(ly,-100,100,-EYE_MOVE_MAX,EYE_MOVE_MAX);
  rightEyeOffsetX = map(rx,-100,100,-EYE_MOVE_MAX,EYE_MOVE_MAX);
  rightEyeOffsetY = map(ry,-100,100,-EYE_MOVE_MAX,EYE_MOVE_MAX);

  if(lx||ly||rx||ry){
    eyeState=NORMAL;
    idleTimer=millis();
  }
}

// data display
void displayDataOnOLED(){
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  const char* title = "ARTEMIS DATA";
  int titleLen = strlen(title) * 6;
  int titleX = (128 - titleLen) / 2;
  display.setCursor(titleX, 0);
  display.println(title);

  display.setCursor(0, 14);
  display.print("X: ");
  display.print(X);

  display.setCursor(0, 26);
  display.print("Y: ");
  display.print(Y);

  display.setCursor(0, 38);
  display.print("Z: ");
  display.print(Z);

  display.setCursor(0, 50);
  display.print("R: ");
  display.print(R);

  display.setCursor(50, 14);
  display.print("G: ");
  display.print(G);

  display.setCursor(50, 26);
  display.print("BTN1: ");
  display.print(btn1State);

  display.setCursor(50, 38);
  display.print("BTN2: ");
  display.print(btn2State);

  display.setCursor(50, 50);
  display.print("BTN3: ");
  display.print(btn3State);

  if(btConnected && SerialBT.connected()) {
    drawBluetoothIcon();
  }

  display.display();
}

void setup(){
  Wire.begin(21,22);
  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  display.clearDisplay();
  display.display();

  showLoadingScreen();

  SerialBT.begin("ARTEMIS_REMOTE", true); // true = master/client

  pinMode(BTN1_PIN,INPUT_PULLUP);
  pinMode(BTN2_PIN,INPUT_PULLUP);
  pinMode(BTN3_PIN,INPUT_PULLUP);
  pinMode(SW_GRIP,INPUT_PULLUP);

  CX=analogRead(JOY1_X);
  CY=analogRead(JOY1_Y);
  CZ=analogRead(JOY2_Y);
  CR=analogRead(JOY2_X);

  idleTimer=millis();
  stateTimer=millis();
}

void loop(){
  static bool lastBtn1State=HIGH;
  bool btn1Now=digitalRead(BTN1_PIN);
  if(lastBtn1State==HIGH && btn1Now==LOW){
      slideMode++;
      if(slideMode>1) slideMode=0;
  }
  lastBtn1State=btn1Now;

  btn2State=digitalRead(BTN2_PIN)==LOW;
  btn3State=digitalRead(BTN3_PIN)==LOW;
  static bool lastGrip=HIGH;
  bool gripNow=digitalRead(SW_GRIP);
  if(lastGrip && !gripNow) G=!G;
  lastGrip=gripNow;

  int jx=readJoystick(JOY1_X,CX);
  int jy=readJoystick(JOY1_Y,CY,true);
  int jz=readJoystick(JOY2_Y,CZ);
  int jr=readJoystick(JOY2_X,CR);

  if(jx) X+=(jx>0?STEP:-STEP);
  if(jy) Y+=(jy>0?STEP:-STEP);
  if(jz) Z+=(jz>0?STEP:-STEP);
  if(jr) R+=(jr>0?STEP_R:-STEP_R);

  X=constrain(X,X_MIN,X_MAX);
  Y=constrain(Y,Y_MIN,Y_MAX);
  Z=constrain(Z,Z_MIN,Z_MAX);
  R=constrain(R,R_MIN,R_MAX);

  switch(slideMode){
    case 0: updateEyeFromJoystick(); updateEyes(); break;
    case 1: displayDataOnOLED(); break;
  }

  handleBluetoothConnect();
  sendDataBluetooth();

  delay(40);
}