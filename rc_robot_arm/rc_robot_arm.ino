#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

/* ================= OLED ================= */
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

/* ================= PIN ================= */
#define JOY1_X 35
#define JOY1_Y 34
#define JOY2_X 33
#define JOY2_Y 32

#define BTN1_PIN 5
#define BTN2_PIN 17
#define BTN3_PIN 18
#define SW_GRIP 15

/* ================= PARAM ================= */
#define DEADZONE 120
#define EYE_MOVE_MAX 6
#define STEP 2
#define STEP_R 2

/* ================= WORKSPACE ================= */
#define X_MIN -100
#define X_MAX 100
#define Y_MIN 30
#define Y_MAX 150
#define Z_MIN -50
#define Z_MAX 150
#define R_MIN -180
#define R_MAX 180

/* ================= CENTER ================= */
int CX, CY, CZ, CR;

/* ================= STATE ================= */
int X=0, Y=50, Z=0, R=0;
int G=0;
int btn1State=0, btn2State=0, btn3State=0;

/* ================= SLIDE MODE ================= */
int slideMode=0; // 0=animasi,1=joystick,2=data

/* ================= EYE GEOMETRY ================= */
#define EYE_W 26
#define EYE_H 36
#define EYE_Y 18
#define EYE_GAP 16
#define LEFT_EYE_X (64 - EYE_GAP/2 - EYE_W)
#define RIGHT_EYE_X (64 + EYE_GAP/2)

/* ================= TIMING ================= */
#define BLINK_TIME 1000
#define IDLE_DURATION 20000
#define LOOK_TIME 2000
#define TRANSITION_TIME 400
#define SMILE_TIME 1400

/* ================= FSM ================= */
enum EyeState {NORMAL, BLINK, LOOK, TRANSITION, SMILE};
EyeState eyeState=NORMAL;
unsigned long stateTimer=0;
unsigned long idleTimer=0;
bool lookRight=true;

/* ================= EYE OFFSET ================= */
int leftEyeOffsetX=0, leftEyeOffsetY=0, rightEyeOffsetX=0, rightEyeOffsetY=0;

/* ================= FUNCTIONS ================= */
int readJoystick(int pin,int center,bool invert=false){
  int raw=analogRead(pin);
  int delta=raw-center;
  if(abs(delta)<DEADZONE) return 0;
  int val=map(delta,-2048,2047,-100,100);
  return invert?-val:val;
}

void showLoadingScreen(unsigned long durationMs = 2000) {
  const char* text = "Loading . . .";
  int textLen = strlen(text) * 6;
  int textX = (128 - textLen) / 2;
  int textY = 18;

  int barX = 14;
  int barY = 38;
  int barWidth = 100;
  int barHeight = 8;

  int blocks = 10;                         // jumlah kotak █
  int blockW = barWidth / blocks;

  unsigned long start = millis();

  while (millis() - start < durationMs) {
    display.clearDisplay();

    // === Text ===
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(textX, textY);
    display.print(text);

    // === Outline bar ===
    display.drawRect(barX, barY, barWidth, barHeight, SSD1306_WHITE);

    // === Progress animation ===
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

/* ======== DRAW EYES ======== */
void drawNormalEyes(){
  display.fillRoundRect(LEFT_EYE_X + leftEyeOffsetX, EYE_Y + leftEyeOffsetY, EYE_W, EYE_H, 10, SSD1306_WHITE);
  display.fillRoundRect(RIGHT_EYE_X + rightEyeOffsetX, EYE_Y + rightEyeOffsetY, EYE_W, EYE_H, 10, SSD1306_WHITE);
}
void drawBlinkEyes(){
  display.fillRoundRect(LEFT_EYE_X, EYE_Y + EYE_H/2, EYE_W, 5, 2, SSD1306_WHITE);
  display.fillRoundRect(RIGHT_EYE_X, EYE_Y + EYE_H/2, EYE_W, 5, 2, SSD1306_WHITE);
}
void drawLookEyes(){
  int offset = lookRight?12:-12;
  display.fillRoundRect(LEFT_EYE_X+offset, EYE_Y, EYE_W, EYE_H, 10, SSD1306_WHITE);
  display.fillRoundRect(RIGHT_EYE_X+offset, EYE_Y, EYE_W, EYE_H, 10, SSD1306_WHITE);
}
void drawSmileEye(int cx,int cy){
  for(int x=-12;x<=12;x++){
    int y=(x*x)/28;
    for(int t=0;t<8;t++){
      display.drawPixel(cx + x, cy + y + t, SSD1306_WHITE);
      display.drawPixel(cx + x + 1, cy + y + t, SSD1306_WHITE);
    }
  }
}
void drawSmileEyes(){
  int y = EYE_Y + EYE_H/2 - 14;
  drawSmileEye(LEFT_EYE_X+EYE_W/2, y);
  drawSmileEye(RIGHT_EYE_X+EYE_W/2, y);
}

/* ======== UPDATE EYE FSM ======== */
void updateEyes(){
  unsigned long now=millis();
  display.clearDisplay();
  switch(eyeState){
    case NORMAL: drawNormalEyes();
      if(now - stateTimer>2500){eyeState=BLINK;stateTimer=now;}
      if(now - idleTimer>IDLE_DURATION){eyeState=LOOK;lookRight=true;stateTimer=now;}
      break;
    case BLINK: drawBlinkEyes();
      if(now - stateTimer>BLINK_TIME){eyeState=NORMAL;stateTimer=now;}
      break;
    case LOOK: drawLookEyes();
      if(now - stateTimer>LOOK_TIME/2) lookRight=false;
      if(now - stateTimer>LOOK_TIME){eyeState=TRANSITION;stateTimer=now;}
      break;
    case TRANSITION: drawNormalEyes();
      if(now - stateTimer>TRANSITION_TIME){eyeState=SMILE;stateTimer=now;}
      break;
    case SMILE: drawSmileEyes();
      if(now - stateTimer>SMILE_TIME){eyeState=NORMAL;idleTimer=now;stateTimer=now;}
      break;
  }
  display.display();
}

/* ======== UPDATE EYE FROM JOYSTICK ======== */
void updateEyeFromJoystick(){
  int lx = readJoystick(JOY1_X, CX);       // X kiri tetap
  int ly = readJoystick(JOY1_Y, CY);       // Y kiri → searah joystick
  int rx = readJoystick(JOY2_X, CR);       // X kanan tetap
  int ry = readJoystick(JOY2_Y, CZ);       // Y kanan → searah joystick

  leftEyeOffsetX  = map(lx,-100,100,-EYE_MOVE_MAX,EYE_MOVE_MAX);
  leftEyeOffsetY  = map(ly,-100,100,-EYE_MOVE_MAX,EYE_MOVE_MAX);
  rightEyeOffsetX = map(rx,-100,100,-EYE_MOVE_MAX,EYE_MOVE_MAX);
  rightEyeOffsetY = map(ry,-100,100,-EYE_MOVE_MAX,EYE_MOVE_MAX);

  if(lx||ly||rx||ry){ eyeState=NORMAL; idleTimer=millis(); }
}


/* ======== DISPLAY DATA ======== */
void displayDataOnOLED(){
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // ===== Judul (CENTER) =====
  const char* title = "ARTEMIS REMOTE DATA";
  int titleLen = strlen(title) * 6;               // 6 px per char
  int titleX = (128 - titleLen) / 2;
  display.setCursor(titleX, 0);
  display.println(title);

  // ===== Kolom kiri =====
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

  // ===== Kolom kanan =====
  display.setCursor(72, 14);
  display.print("G: ");
  display.print(G);

  display.setCursor(72, 26);
  display.print("BTN1: ");
  display.print(btn1State);

  display.setCursor(72, 38);
  display.print("BTN2: ");
  display.print(btn2State);

  display.setCursor(72, 50);
  display.print("BTN3: ");
  display.print(btn3State);

  display.display();
}

/* ================= SETUP ================= */
void setup(){
  Wire.begin(21,22);
  display.begin(SSD1306_SWITCHCAPVCC,OLED_ADDR);
  display.clearDisplay();
  display.display();
  delay(1500);

  showLoadingScreen(2500);

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

/* ================= LOOP ================= */
void loop(){
  // tombol slide
  static bool lastBtn1State=HIGH;
  bool btn1Now=digitalRead(BTN1_PIN);
  if(lastBtn1State==HIGH && btn1Now==LOW){
      slideMode++;
      if(slideMode>1) slideMode=0;
  }
  lastBtn1State=btn1Now;

  // tombol lain & grip
  btn2State=digitalRead(BTN2_PIN)==LOW;
  btn3State=digitalRead(BTN3_PIN)==LOW;
  static bool lastGrip=HIGH;
  bool gripNow=digitalRead(SW_GRIP);
  if(lastGrip && !gripNow) G=!G;
  lastGrip=gripNow;

  // update X,Y,Z,R
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

  // pilih mode slide
  switch(slideMode){
    case 0: updateEyeFromJoystick(); updateEyes(); break;  // joystick control realtime
    case 1: displayDataOnOLED(); break;            // data display
  }

  delay(40);
}
