#include "IMUsManager.h"
#include "Multiplexer.h"
#include <Wire.h>
#include <tuple>        // std::tuple, std::get, std::tie, std::ignore

void IMUsManager::operator[](int i) {
  lookAt(i);
}

void IMUsManager::lookAt(int i) {
  if (i > 7) {
    DEBUG_PRINT.println("Channel index '" + String(i) + "' out of bounds.");
  }

  if (currentIndex != i) {
    currentIndex = i;
    mp.set(i);
    extend = extends[i];
    imu1[i];
    imu2[i];
  }
}

void IMUsManager::calibrateChannel(int channel) {
  DEBUG_PRINT.println("Calibrating IMUs on multiplexer channel " + String(channel));

  mp.set(channel);

  byte error;

  Wire.beginTransmission(i2c_addr_default);
  error = Wire.endTransmission();
  if (error == 0) {
    imu1.Calibrate();

    imu1.imu.setAccelRange(16);
    imu1.imu.setGyroRange(2000);
  }

  Wire.beginTransmission(i2c_addr_jumper);
  error = Wire.endTransmission();
  if (error == 0) {
    imu2.Calibrate();

    imu2.imu.setAccelRange(16);
    imu2.imu.setGyroRange(2000);
  }
}

void IMUsManager::calibrateAllChannels() {
  for (int i = 0; i < 8; i++) {
    try {
      calibrateChannel(i);
    }
    catch (String error) {
      DEBUG_PRINT.println(error);
    }
  }
}

std::tuple<bool, bool> IMUsManager::initChannel(int channel) {

  DEBUG_PRINT.println("Initializing IMUs on multiplexer channel " + String(channel));

  byte error, address;

  mp.set(channel);

  Wire.beginTransmission(i2c_addr_default);
  error = Wire.endTransmission();
  if (error == 0) {
    imu1.Calibrate();
    if (imu1.start(i2c_addr_default)) {
      imu1Good = true;
      DEBUG_PRINT.println("imu1 good");
    } else {
      DEBUG_PRINT.println("Error in multiplexer channel " + String(channel) + String(error) + " at address " + String(address, HEX));
    }
  }

  Wire.beginTransmission(i2c_addr_jumper);
  error = Wire.endTransmission();
  if (error == 0) {
    imu2.Calibrate();
    if (imu2.start(i2c_addr_jumper)) {
      imu2Good = true;
      DEBUG_PRINT.println("imu2 good");
    } else {
      DEBUG_PRINT.println("Error in multiplexer channel " + String(channel) + String(error) + " at address " + String(address, HEX));
    }
  }

  extends[channel] = imu1Good && imu2Good;

  return std::tuple<bool, bool>(imu1Good, imu2Good);
}

void IMUsManager::initAllChannels() {
  for (int i = 0; i < 8; i++) {
    try {
      initChannel(i);
    }
    catch (String error) {
      DEBUG_PRINT.println(error);
    }
  }
}
