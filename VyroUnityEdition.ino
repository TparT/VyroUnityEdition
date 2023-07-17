#include <esp_wifi.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <WiFiUdp.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include <Wire.h>
#include "IMUsManager.h"

#include "esp32_settings.h"
#include "debugLogger.h"

#include "pitches.h"

const char *PARAM_TARGET_IP = "targetIp";
String targetIp;

// HTML web page to handle 1 input fields (targetIp)
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>ESP Input Form</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
  .button {
  background-color: #4CAF50;
  border: none;
  color: white;
  padding: 16px 40px;
  text-decoration: none;
  font-size: 30px;
  margin: 2px;
  cursor: pointer;
  }
  </style>
  </head><body>
  <form action="/post">
    Target IP: <input type="text" name="targetIp">
    <input type="submit" value="Submit">
  </form><br>
  <p><a href="/calibrate"><button class="button">CALIBRATE</button></a></p>
</body></html>)rawliteral";

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}


#define LFOOT 1      /* כף רגל שמאל */
#define RFOOT 2      /* כף רגל ימין */
#define LSHIN 3      /* שוק שמאלית */
#define RSHIN 4      /* שוק ימנית */
#define LTHIGH 5     /* ירך שמאלית */
#define RTHIGH 6     /* ירך ימנית */
#define WAIST 7      /* מותן */
#define CHEST 8      /* חזה */
#define LSHOULDER 9  /* כתף שמאל */
#define RSHOULDER 10 /* כתף ימין */
#define LUPPERARM 11 /* שריר יד שמאל עליון */
#define RUPPERARM 12 /* שריר יד ימין עליון */

class Vyro {
public:
  IMUsManager trackers;

  int currentMainIndex = -1;

  void setMainMultiplexer(int channel) {

    if (currentMainIndex != channel) {
      DEBUG_PRINT.println("========== SETTING MAIN MULTIPLEXER TO CHANNEL " + String(channel) + "! ==========");

      currentMainIndex = channel;

      Wire.beginTransmission(0x71);  // Main TCA9548A address is 0x71
      Wire.write(1 << channel);      // Send byte to select bus
      int result = Wire.endTransmission();

      switch (result) {
        case 0: DEBUG_PRINT.println("Main Multiplexer successfully changed to channel " + String(channel) + "!"); break;
        case 1: DEBUG_PRINT.println("Main Multiplexer errored (data too long to fit in transmit buffer) while changing to channel " + String(channel) + "!"); break;
        case 2: DEBUG_PRINT.println("Main Multiplexer errored (received NACK on transmit of address) while changing to channel " + String(channel) + "!"); break;
        case 3: DEBUG_PRINT.println("Main Multiplexer errored (received NACK on transmit of data) while changing to channel " + String(channel) + "!"); break;
        case 4: DEBUG_PRINT.println("Main Multiplexer errored (other error) while changing to channel " + String(channel) + "!"); break;
        case 5: DEBUG_PRINT.println("Main Multiplexer errored (timeout) while changing to channel " + String(channel) + "!"); break;
        default: DEBUG_PRINT.println("Main Multiplexer errored (other error [on the default case]) while changing to channel " + String(channel) + "!"); break;
      }
    }
  }
};

struct TrackerData {
  uint8_t id = 0;

  int16_t x = 0;
  int16_t y = 0;
  int16_t z = 0;
  int16_t w = 32767;

  void setQuat(int16_t qx, int16_t qy, int16_t qz, int16_t qw = 32767) {
    x = qx;
    y = qy;
    z = qz;
    w = qw;
  }
};

Vyro vyro;
TrackerData trackers[13];  // Trackers data

WiFiManager wifiManager;
AsyncWebServer server(80);  // Set web server port number to 80
const char *host = "VYRO";

// UDP destination address, these will be changed according to the configuration
IPAddress outIp(192, 168, 1, 114);  // 127.0.0.1
const unsigned int inPort = 8000, outPort = 9000;
WiFiUDP Udp;

uint16_t serialLength;

int16_t map(float x, float in_min = -1, float in_max = 1, float out_min = -32767, float out_max = 32767) {
  return (int16_t)((x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min);
}

void setup() {
  // put your setup code here, to run once:

  buzz(NOTE_F6, 250);
  buzz(NOTE_F7, 250);
  buzz(NOTE_A7, 250);

  Serial.begin(115200);
  while (!Serial);  // wait for the serial port to open
  DEBUG_PRINT.println("Serial set up!");

  WiFi.hostname(host);
  WiFi.begin();

  Wire.begin(32 /*SDA Pin*/, 33 /*SCL Pin*/, 400000 /*Speed*/);
  DEBUG_PRINT.println("Wire set up! [ SDA = 32 , SCL = 33 , SPEED = 400000 ]");

  Udp.begin(inPort);

  if (WiFi.status() != WL_CONNECTED)
    Serial.println("WiFi not connected");

  // wifiManager.setAPStaticIPConfig(IPAddress(192, 168, 1, 69), IPAddress(192, 168, 69, 69), IPAddress(255, 255, 255, 0));
  wifiManager.setConfigPortalBlocking(false);
  wifiManager.setConfigPortalTimeout(60);

  if (wifiManager.autoConnect(host)) {
    Serial.println("connected...yeey :)");
  } else {
    Serial.println("Configportal running");
  }

  if (WiFi.status() == WL_CONNECTED) {
    buzzResult(true);
    Serial.println("WiFi connected!");
  } else {
    buzzResult(false);
  }

  /*use mdns for host name resolution*/
  if (!MDNS.begin(host)) {  //http://vyro.local
    Serial.println("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
    }
  }

  // Send web page with input fields to client
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });

  // Send a POST request to <ESP_IP>/post?targetIp=<inputMessage>
  server.on("/post", HTTP_POST, [](AsyncWebServerRequest *request) {
    String inputMessage;
    String inputParam;
    // POST targetIp value on <ESP_IP>/post?targetIp=<inputMessage>
    if (request->hasParam(PARAM_TARGET_IP)) {
      inputMessage = request->getParam(PARAM_TARGET_IP)->value();
      outIp.fromString(inputMessage);
      inputParam = PARAM_TARGET_IP;
    } else {
      inputMessage = "No message sent";
      inputParam = "none";
    }

    DEBUG_PRINT.println(inputMessage);
    request->send(200, "text/html", "HTTP POST request sent to your ESP on input field (" + inputParam + ") with value: " + inputMessage + "<br><a href=\"/\">Return to Home Page</a>");
  });

  server.on("/calibrate", HTTP_GET, [](AsyncWebServerRequest *request) {
    DEBUG_PRINT.println("RECEIVED CALIBRATION REQUEST!!!");
    request->send(200, "text/html", "HTTP POST request sent to your ESP for calibration<br><a href=\"/\">Return to Home Page</a>");
    Calibrate();
  });

  server.onNotFound(notFound);
  server.begin();

  if (!brown_en) {
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  }

  setCpuFrequencyMhz(80);

  vyro.setMainMultiplexer(1);
  vyro.trackers.initAllChannels();
  vyro.setMainMultiplexer(2);
  vyro.trackers.initAllChannels();

  DEBUG_PRINT.println("All channels are supposed to be initialized!");

  pinMode(led_pin, OUTPUT);

  if (!brown_en) {
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 1);
  }

  DEBUG_PRINT.println("Finished setup!");

  buzzResult(true);
  //PlayTetris();
}

void sendUDP(String payload) {
  Udp.beginPacket(outIp, outPort);
  Udp.write((uint8_t *)payload.c_str(), payload.length());
  Udp.endPacket();
}

String formatQuats(Madgwick quat1, Madgwick quat2, Madgwick quat3, Madgwick quat4) {
  String formatted;

  formatted += String(quat1.getQuatX(), 4);
  formatted += ",";
  formatted += String(quat1.getQuatY(), 4);
  formatted += ",";
  formatted += String(quat1.getQuatZ(), 4);
  formatted += ",";
  formatted += String(quat1.getQuatW(), 4);

  formatted += "|";

  formatted += String(quat2.getQuatX(), 4);
  formatted += ",";
  formatted += String(quat2.getQuatY(), 4);
  formatted += ",";
  formatted += String(quat2.getQuatZ(), 4);
  formatted += ",";
  formatted += String(quat2.getQuatW(), 4);

  formatted += "|";

  formatted += String(quat3.getQuatX(), 4);
  formatted += ",";
  formatted += String(quat3.getQuatY(), 4);
  formatted += ",";
  formatted += String(quat3.getQuatZ(), 4);
  formatted += ",";
  formatted += String(quat3.getQuatW(), 4);

  formatted += "|";

  formatted += String(quat4.getQuatX(), 4);
  formatted += ",";
  formatted += String(quat4.getQuatY(), 4);
  formatted += ",";
  formatted += String(quat4.getQuatZ(), 4);
  formatted += ",";
  formatted += String(quat4.getQuatW(), 4);

  return formatted;
}

void printQuat(TrackerData d, bool last = false) {
  Serial.print(d.id);
  Serial.print(",");
  Serial.print(d.x);
  Serial.print(",");
  Serial.print(d.y);
  Serial.print(",");
  Serial.print(d.z);
  Serial.print(",");
  Serial.print(d.w);
  if (last)
    Serial.println();
  else
    Serial.print("|");
}

void buzzResult(bool success) {
  if (success) {
    buzz(NOTE_DS8, 200);
    delay(10);
    buzz(NOTE_DS8, 200);
  } else {
    buzz(NOTE_FS4, 500);
    delay(10);
    buzz(NOTE_FS4, 500);
  }
}

void PlayRasputin() {
  buzz(NOTE_FS6, 200);
  delay(100);
  buzz(NOTE_FS6, 200);
  delay(100);
  buzz(NOTE_FS6, 200);
  delay(300);
  buzz(NOTE_FS6, 200);
  delay(100);
  buzz(NOTE_G6, 100);
  delay(100);
  buzz(NOTE_A6, 100);
  delay(100);
  buzz(NOTE_G6, 100);
  delay(100);
  buzz(NOTE_FS6, 100);
  delay(100);
  buzz(NOTE_FS6, 100);
  delay(200);
  buzz(NOTE_CS6, 100);
  delay(100);
  buzz(NOTE_D6, 100);
  delay(100);
  buzz(NOTE_E6, 100);
  delay(100);
  buzz(NOTE_E6, 100);
  delay(100);
  buzz(NOTE_E6, 100);
  delay(200);
  buzz(NOTE_CS6, 100);
  delay(100);
  buzz(NOTE_D6, 100);
  delay(100);
  buzz(NOTE_E6, 100);
  delay(100);
  buzz(NOTE_E6, 100);
  delay(100);
  buzz(NOTE_E6, 100);
  delay(100);
  buzz(NOTE_E6, 100);
  delay(10);
  buzz(NOTE_D6, 100);
  delay(100);
  buzz(NOTE_CS6, 100);
  delay(100);
  buzz(NOTE_B5, 100);
}

void PlayTetris() {
  int tempo = 144;

  // notes of the moledy followed by the duration.
  // a 4 means a quarter note, 8 an eighteenth , 16 sixteenth, so on
  // !!negative numbers are used to represent dotted notes,
  // so -4 means a dotted quarter note, that is, a quarter plus an eighteenth!!
  int melody[] = {

    //Based on the arrangement at https://www.flutetunes.com/tunes.php?id=192

    NOTE_E5, 4, NOTE_B4, 8, NOTE_C5, 8, NOTE_D5, 4, NOTE_C5, 8, NOTE_B4, 8,
    NOTE_A4, 4, NOTE_A4, 8, NOTE_C5, 8, NOTE_E5, 4, NOTE_D5, 8, NOTE_C5, 8,
    NOTE_B4, -4, NOTE_C5, 8, NOTE_D5, 4, NOTE_E5, 4,
    NOTE_C5, 4, NOTE_A4, 4, NOTE_A4, 8, NOTE_A4, 4, NOTE_B4, 8  //,  NOTE_C5,8,

    // NOTE_D5, -4,  NOTE_F5,8,  NOTE_A5,4,  NOTE_G5,8,  NOTE_F5,8,
    // NOTE_E5, -4,  NOTE_C5,8,  NOTE_E5,4,  NOTE_D5,8,  NOTE_C5,8,
    // NOTE_B4, 4,  NOTE_B4,8,  NOTE_C5,8,  NOTE_D5,4,  NOTE_E5,4,
    // NOTE_C5, 4,  NOTE_A4,4,  NOTE_A4,4, REST, 4,

    // NOTE_E5, 4,  NOTE_B4,8,  NOTE_C5,8,  NOTE_D5,4,  NOTE_C5,8,  NOTE_B4,8,
    // NOTE_A4, 4,  NOTE_A4,8,  NOTE_C5,8,  NOTE_E5,4,  NOTE_D5,8,  NOTE_C5,8,
    // NOTE_B4, -4,  NOTE_C5,8,  NOTE_D5,4,  NOTE_E5,4,
    // NOTE_C5, 4,  NOTE_A4,4,  NOTE_A4,8,  NOTE_A4,4,  NOTE_B4,8,  NOTE_C5,8,

    // NOTE_D5, -4,  NOTE_F5,8,  NOTE_A5,4,  NOTE_G5,8,  NOTE_F5,8,
    // NOTE_E5, -4,  NOTE_C5,8,  NOTE_E5,4,  NOTE_D5,8,  NOTE_C5,8,
    // NOTE_B4, 4,  NOTE_B4,8,  NOTE_C5,8,  NOTE_D5,4,  NOTE_E5,4,
    // NOTE_C5, 4,  NOTE_A4,4,  NOTE_A4,4, REST, 4,


    // NOTE_E5,2,  NOTE_C5,2,
    // NOTE_D5,2,   NOTE_B4,2,
    // NOTE_C5,2,   NOTE_A4,2,
    // NOTE_GS4,2,  NOTE_B4,4,  REST,8,
    // NOTE_E5,2,   NOTE_C5,2,
    // NOTE_D5,2,   NOTE_B4,2,
    // NOTE_C5,4,   NOTE_E5,4,  NOTE_A5,2,
    // NOTE_GS5,2,

  };

  // sizeof gives the number of bytes, each int value is composed of two bytes (16 bits)
  // there are two values per note (pitch and duration), so for each note there are four bytes
  int notes = sizeof(melody) / sizeof(melody[0]) / 2;

  // this calculates the duration of a whole note in ms (60s/tempo)*4 beats
  int wholenote = (60000 * 4) / tempo;

  int divider = 0, noteDuration = 0;

  // iterate over the notes of the melody.
  // Remember, the array is twice the number of notes (notes + durations)
  for (int thisNote = 0; thisNote < notes * 2; thisNote = thisNote + 2) {

    // calculates the duration of each note
    divider = melody[thisNote + 1];
    if (divider > 0) {
      // regular note, just proceed
      noteDuration = (wholenote) / divider;
    } else if (divider < 0) {
      // dotted notes are represented with negative durations!!
      noteDuration = (wholenote) / abs(divider);
      noteDuration *= 1.5;  // increases the duration in half for dotted notes
    }

    // we only play the note for 90% of the duration, leaving 10% as a pause
    tone(BUZZZER_PIN, melody[thisNote], noteDuration * 0.9);

    // Wait for the specief duration before playing the next note.
    delay(noteDuration);

    // stop the waveform generation before the next note.
    noTone(BUZZZER_PIN);
  }
}

void Calibrate() {
  vyro.setMainMultiplexer(1);
  vyro.trackers.calibrateAllChannels();
  vyro.setMainMultiplexer(2);
  vyro.trackers.calibrateAllChannels();
}

void loop() {
  // put your main code here, to run repeatedly:

  wifiManager.process();
  serialLength = Serial.available();
  if (serialLength) {
    String cmd = Serial.readString();  //read until timeout
    cmd.trim();
    if (cmd == "c") {
      Calibrate();
    }
  }

  vyro.setMainMultiplexer(1);
  vyro.trackers.lookAt(7);

  Madgwick quaternion_LUPPERARM = vyro.trackers.imu1.getQuaternion();
  trackers[LUPPERARM].setQuat(map(quaternion_LUPPERARM.getQuatX()), map(quaternion_LUPPERARM.getQuatY()), map(quaternion_LUPPERARM.getQuatZ()), map(quaternion_LUPPERARM.getQuatW()));

  Madgwick quaternion_LSHOULDER = vyro.trackers.imu2.getQuaternion();
  trackers[LSHOULDER].setQuat(map(quaternion_LSHOULDER.getQuatX()), map(quaternion_LSHOULDER.getQuatY()), map(quaternion_LSHOULDER.getQuatZ()), map(quaternion_LSHOULDER.getQuatW()));

  vyro.setMainMultiplexer(2);
  vyro.trackers.lookAt(2);

  Madgwick quaternion_LSHIN = vyro.trackers.imu1.getQuaternion();
  trackers[LSHIN].setQuat(map(quaternion_LSHIN.getQuatX()), map(quaternion_LSHIN.getQuatY()), map(quaternion_LSHIN.getQuatZ()), map(quaternion_LSHIN.getQuatW()));

  Madgwick quaternion_LTHIGH = vyro.trackers.imu2.getQuaternion();
  trackers[LTHIGH].setQuat(map(quaternion_LTHIGH.getQuatX()), map(quaternion_LTHIGH.getQuatY()), map(quaternion_LTHIGH.getQuatZ()), map(quaternion_LTHIGH.getQuatW()));

  String formattedQuats = formatQuats(quaternion_LUPPERARM, quaternion_LSHOULDER, quaternion_LTHIGH, quaternion_LSHIN);

  Serial.println(formattedQuats);
  sendUDP(formattedQuats);

  delay(50);
}
