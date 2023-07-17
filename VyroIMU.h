#include "FastIMU.h"
#include "Madgwick.h"
#include "debugLogger.h"
#include "Arduino.h"

const int i2c_addr_default = 0x69;
const int i2c_addr_jumper = 0x68;

#define ACCEL_THRESHOLD 0.5

struct VyroImuData {

  calData calib = { 0 };  //Calibration data

  AccelData IMUAccel;  //Sensor data
  GyroData IMUGyro;

  Madgwick filter;

//  bool motionDetected() {
//    float calc = (fabs(IMUAccel.accelX) + fabs(IMUAccel.accelY) + fabs(IMUAccel.accelZ));
//    Serial.println("calc = " + String(calc, 4));
//    return calc > ACCEL_THRESHOLD;
//  }

  void Calibrate(BMI160 imu) {
    imu.calibrateAccelGyro(&calib);
  }
  
  Madgwick getQuaternion(BMI160 imu) {

    imu.update();
    imu.getAccel(&IMUAccel);
    imu.getGyro(&IMUGyro);

    filter.updateIMU(IMUGyro.gyroX, IMUGyro.gyroY, IMUGyro.gyroZ, IMUAccel.accelX, IMUAccel.accelY, IMUAccel.accelZ);

    return filter;
  }
};

class VyroIMU {
public:

  VyroImuData IMUsData[8];
  int currentImuIndex;

  BMI160 imu;

  void operator[](int i);

  bool start(bool jumper);
  bool start(int address);
  bool startAndApplySettings(bool jumper);
  bool startAndApplySettings(int address);

  void Calibrate();

  Madgwick getQuaternion();
};
