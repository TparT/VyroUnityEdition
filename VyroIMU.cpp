#include "VyroIMU.h"

void VyroIMU::operator[](int i) {
  if (i > 7) {
    DEBUG_PRINT.println("Channel index out of bounds");
  }
  currentImuIndex = i;
}

bool VyroIMU::start(bool jumper) {
  return imu.init(IMUsData[currentImuIndex].calib, jumper ? i2c_addr_jumper : i2c_addr_default);
}

bool VyroIMU::start(int address) {
  return imu.init(IMUsData[currentImuIndex].calib, address);
}

void VyroIMU::Calibrate() {
  IMUsData[currentImuIndex].Calibrate(imu);
}

Madgwick VyroIMU::getQuaternion() {
  return IMUsData[currentImuIndex].getQuaternion(imu);
}
