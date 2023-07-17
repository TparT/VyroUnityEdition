#include "debugLogger.h"
#include "VyroIMU.h"

class IMUsManager {
  public:
  VyroIMU imu1;
  VyroIMU imu2;

  bool imu1Good = false;
  bool imu2Good = false;

  bool extends[8];
  bool extend = false;

  int currentIndex = -1;

  void operator[](int i);
  void lookAt(int i);
  void calibrateChannel(int channel);
  void calibrateAllChannels();
  std::tuple<bool, bool> initChannel(int channel);
  void initAllChannels();
};
