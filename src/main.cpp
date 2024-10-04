/*
データシートを参照
https://www.infineon.com/dgdl/Infineon-DPS310-DataSheet-v01_02-EN.pdf?fileId=5546d462576f34750157750826c42242
*/

#include <Arduino.h>
#include <Wire.h>

#include "pins.h"

#define DPS310_ADDR 0x77

// キャリブレーション係数
int32_t c00, c10;
int32_t c20, c30, c01, c11, c21;

// スケーリング係数の指定(oversampling回数によって異なる)
float kT = 524288.0;
float kP = 1040384.0;

// レジスタにデータの書き込みを行う
void writeDPS310Register(uint8_t regAddr, uint8_t value) {
  Wire.beginTransmission(DPS310_ADDR);
  Wire.write(regAddr);
  Wire.write(value);
  Wire.endTransmission();
}

// レジスタにデータの読み出しを行う
int32_t readDPS310Register(uint8_t regAddr) {
  Wire.beginTransmission(DPS310_ADDR);
  Wire.write(regAddr);
  Wire.endTransmission();
  Wire.requestFrom(DPS310_ADDR, 1);
  return Wire.read();
}

void setup() {
  Serial.begin(115200);
  
  Wire.begin();
  
  // キャリブレーション係数を取得
  // c00 [19:12]
  int32_t regBit2 = readDPS310Register(0x13);

  // c00 [11:4]
  int32_t regBit1 = readDPS310Register(0x14);

  // c00 [3:0] c10 [19:16]
  int32_t regBit0 = readDPS310Register(0x15);
  c00 = (regBit2 << 12) | (regBit1 << 4) | ((regBit0 & 0xF0) >> 4);

  // c10 [15:8]
  regBit2 = readDPS310Register(0x16);
  
  // c10 [7:0]
  regBit1 = readDPS310Register(0x17);
  c10 = ((regBit0 & 0x0F) << 16) | (regBit2 << 8) | regBit1;

  // c01 [15:8]
  regBit1 = readDPS310Register(0x18);
  
  // c01 [7~0]
  regBit0 = readDPS310Register(0x19);
  c01 = (regBit1 << 8) | regBit0;

  // c11 [15:8]
  regBit1 = readDPS310Register(0x1A);
  
  // c11 [7~0]
  regBit0 = readDPS310Register(0x1B);
  c11 = (regBit1 << 8) | regBit0;

  // c20 [15:8]
  regBit1 = readDPS310Register(0x1C);
  
  // c20 [7~0]
  regBit0 = readDPS310Register(0x1D);
  c20 = (regBit1 << 8) | regBit0;

  // c21 [15:8]
  regBit1 = readDPS310Register(0x1E);

  // c21 [7~0]
  regBit0 = readDPS310Register(0x1F);
  c21 = (regBit1 << 8) | regBit0;
  
  // c30 [15:8]
  regBit1 = readDPS310Register(0x20);
  
  // c30 [7~0]
  regBit0 = readDPS310Register(0x21);
  c30 = (regBit1 << 8) | regBit0;

  // 2の補数表現なので負の値なら負の値に変換する
  if (c00 & (1 << 19)) {
    c00 = c00 - (1 << 20);
  }
  if (c10 & (1 << 19)) {
    c10 = c10 - (1 << 20);
  }
  if (c01 & (1 << 15)) {
    c01 = c01 - (1 << 16);
  }
  if (c11 & (1 << 15)) {
    c11 = c11 - (1 << 16);
  }
  if (c20 & (1 << 15)) {
    c20 = c20 - (1 << 16);
  }
  if (c21 & (1 << 15)) {
    c21 = c21 - (1 << 16);
  }
  if (c30 & (1 << 15)) {
    c30 = c30 - (1 << 16);
  }

  // Pressure Configuration
  // 1秒間に4回, oversampling:64
  // 測定秒数は104.4ms
  // 精度は0.2
  writeDPS310Register(0x06, 0x26);
  delay(10);

  // Temperature Configuration
  // 外部センサ使用
  // 1秒間に4回, oversamping:1
  writeDPS310Register(0x07, 0xA0);
  delay(10);

  // Interrput and FIFO configuration
  // Enable P Shift
  writeDPS310Register(0x09, 0x04);
  delay(10);
}

void loop() {
  // 250ms毎に取得
  unsigned long start_ms = millis();

  // 温度データの取得
  writeDPS310Register(0x08, 0x02);

  // temperature_raw [23:16]
  int32_t t1 = readDPS310Register(0x03);
  delay(1);

  // temperature_raw [15:8]
  int32_t t2 = readDPS310Register(0x04);
  delay(1);

  // temperature_raw [7:0]
  int32_t t3 = readDPS310Register(0x05);
  delay(1);

  // 2の補数表現なので負の値なら負の値に変換する
  int32_t t = (t1 << 16) | (t2 << 8) | t3;
  if (t & (1 << 23)) {
    t -= (1 << 24);
  }

  // 気圧データの取得
  writeDPS310Register(0x08, 0x01);
  delay(1);

  // pressure_raw [23:16]
  int32_t p1 = readDPS310Register(0x00);
  delay(1);

  // pressure_raw [15:8]
  int32_t p2 = readDPS310Register(0x01);
  delay(1);

  // pressure_raw [7:0]
  int32_t p3 = readDPS310Register(0x02);

  // 2の補数表現なので負の値なら負の値に変換する
  int32_t p = (p1 << 16) |  (p2 << 8) | p3;
  if (p & (1 << 23)) {
    p -= (1 << 24);
  }

  // スケーリングされた測定結果を計算する
  float t_raw_sc = t / kT;
  float p_raw_sc = p / kP;

  // 実際の気圧値を計算する
  float pressure = c00 + p_raw_sc * (c10 + p_raw_sc * (c20 + p_raw_sc * c30))
                + t_raw_sc * c01 + t_raw_sc * p_raw_sc * (c11 + p_raw_sc * c21);

  Serial.println(pressure);

  while(millis() - start_ms < 250);
}
