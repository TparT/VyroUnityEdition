#include "Multiplexer.h"
#include "debugLogger.h"
#include <Wire.h>

int Multiplexer::set(int channel) {
  DEBUG_PRINT.println("Setting multiplexer to channel " + String(channel) + " ...");

  Wire.beginTransmission(0x70);   // TCA9548A address is 0x70
  Wire.write(1 << channel);       // send byte to select bus
  int result = Wire.endTransmission();

  switch (result) {
    case 0: DEBUG_PRINT.println("Multiplexer successfully changed to channel " + String(channel) + "!"); break;
    case 1: DEBUG_PRINT.println("Multiplexer errored (data too long to fit in transmit buffer) while changing to channel " + String(channel) + "!"); break;
    case 2: DEBUG_PRINT.println("Multiplexer errored (received NACK on transmit of address) while changing to channel " + String(channel) + "!"); break;
    case 3: DEBUG_PRINT.println("Multiplexer errored (received NACK on transmit of data) while changing to channel " + String(channel) + "!"); break;
    case 4: DEBUG_PRINT.println("Multiplexer errored (other error) while changing to channel " + String(channel) + "!"); break;
    case 5: DEBUG_PRINT.println("Multiplexer errored (timeout) while changing to channel " + String(channel) + "!"); break;
    default: DEBUG_PRINT.println("Multiplexer errored (other error [on the default case]) while changing to channel " + String(channel) + "!"); break;
  }

  return result;
}

Multiplexer mp;
