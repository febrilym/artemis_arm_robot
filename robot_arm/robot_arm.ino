#include <Arduino.h>
#include <BluetoothSerial.h>

BluetoothSerial SerialBT;

// data
int X = 0;
int Y = 0;
int Z = 0;
int R = 0;
int G = 0;

int BTN1 = 0;
int BTN2 = 0;
int BTN3 = 0;

// parsing
void parseData(const String &data) {
  int ix  = data.indexOf("X:");
  int iy  = data.indexOf(",Y:");
  int iz  = data.indexOf(",Z:");
  int ir  = data.indexOf(",R:");
  int ig  = data.indexOf(",G:");
  int ib1 = data.indexOf(",B1:");
  int ib2 = data.indexOf(",B2:");
  int ib3 = data.indexOf(",B3:");

  if (ix>=0 && iy>=0 && iz>=0 && ir>=0 && ig>=0 &&
      ib1>=0 && ib2>=0 && ib3>=0) {

    X = data.substring(ix+2, iy).toInt();
    Y = data.substring(iy+3, iz).toInt();
    Z = data.substring(iz+3, ir).toInt();
    R = data.substring(ir+3, ig).toInt();
    G = data.substring(ig+3, ib1).toInt();

    BTN1 = data.substring(ib1+4, ib2).toInt();
    BTN2 = data.substring(ib2+4, ib3).toInt();
    BTN3 = data.substring(ib3+4).toInt();
  }
}

void setup() {
  Serial.begin(115200);
  SerialBT.begin("ARTEMIS_ARM");
  Serial.println("ARTEMIS ARM READY - Waiting connection...");
}

void loop() {
  if (SerialBT.available()) {
    String data = SerialBT.readStringUntil('\n');
    parseData(data);

    Serial.print("X:"); Serial.print(X);
    Serial.print(" Y:"); Serial.print(Y);
    Serial.print(" Z:"); Serial.print(Z);
    Serial.print(" R:"); Serial.print(R);
    Serial.print(" G:"); Serial.print(G);
    Serial.print(" BTN1:"); Serial.print(BTN1);
    Serial.print(" BTN2:"); Serial.print(BTN2);
    Serial.print(" BTN3:"); Serial.println(BTN3);

    /*
      inverse kinematics
      mapping motor/servo
    */
  }
}
