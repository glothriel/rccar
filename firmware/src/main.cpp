#include <Arduino.h>
#include <ESP32Servo.h>
#include <WebServer.h>
#include <WiFi.h>

#include "pin_config.h"
#include "web_ui.h"

#if __has_include("wifi_config.h")
#include "wifi_config.h"
#else
namespace wifi_config {
constexpr char kSsid[] = "";
constexpr char kPassword[] = "";
} // namespace wifi_config
#endif

struct Command {
  int drive;
  int steering;
  bool hasDrive;
  bool hasSteering;
};

constexpr int scaleMotorCommand(int command) {
  return command == 0
             ? 0
             : (command < 0 ? -1 : 1) *
                   (config::kMotorMinimumPwm +
                    ((command < 0 ? -command : command) - 1) *
                        (255 - config::kMotorMinimumPwm) / 254);
}

static_assert(scaleMotorCommand(0) == 0);
static_assert(scaleMotorCommand(1) == config::kMotorMinimumPwm);
static_assert(scaleMotorCommand(-1) == -config::kMotorMinimumPwm);
static_assert(scaleMotorCommand(255) == 255);
static_assert(scaleMotorCommand(-255) == -255);

class DriveMotor {
public:
  void begin() {
    pinMode(config::kMotorAin1Pin, OUTPUT);
    pinMode(config::kMotorAin2Pin, OUTPUT);
    set(0);
  }

  void enable() { digitalWrite(config::kMotorSleepPin, HIGH); }

  void set(int command) {
    command = scaleMotorCommand(constrain(command, -255, 255));
    analogWrite(config::kMotorAin1Pin, command < 0 ? -command : 0);
    analogWrite(config::kMotorAin2Pin, command > 0 ? command : 0);
  }
};

class Steering {
public:
  void begin() {
    servo_.attach(config::kSteeringPin);
    set(0);
  }

  void set(int command) {
    command = constrain(command, -config::kSteeringMaxCommand,
                        config::kSteeringMaxCommand);
    const int angle = command < 0
                          ? map(command, -100, 0,
                                config::kSteeringMinDegrees,
                                config::kSteeringCenterDegrees)
                          : map(command, 0, 100,
                                config::kSteeringCenterDegrees,
                                config::kSteeringMaxDegrees);
    servo_.write(angle);
  }

private:
  Servo servo_;
};

class Transport {
public:
  virtual void begin() = 0;
  virtual bool poll(Command &command) = 0;
  virtual ~Transport() = default;
};

class SerialTransport final : public Transport {
public:
  void begin() override {
    Serial.begin(config::kSerialBaud);
    Serial.println("READY");
  }

  bool poll(Command &command) override {
    while (Serial.available()) {
      const char c = static_cast<char>(Serial.read());
      if (c == '\r') {
        continue;
      }
      if (c == '\n') {
        buffer_[length_] = '\0';
        const bool valid = parse(command);
        length_ = 0;
        return valid;
      }
      if (length_ < sizeof(buffer_) - 1) {
        buffer_[length_++] = c;
      } else {
        length_ = 0;
      }
    }
    return false;
  }

private:
  bool parse(Command &command) const {
    int value;
    char extra;
    if (sscanf(buffer_, "D %d %c", &value, &extra) == 1) {
      command = {value, 0, true, false};
      return true;
    }
    if (sscanf(buffer_, "S %d %c", &value, &extra) == 1) {
      command = {0, value, false, true};
      return true;
    }
    return false;
  }

  char buffer_[24]{};
  size_t length_ = 0;
};

class WebTransport final : public Transport {
public:
  void begin() override {
    if (wifi_config::kSsid[0] == '\0') {
      Serial.println("WIFI DISABLED: create include/wifi_config.h");
      return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.setHostname("rccar");
    WiFi.begin(wifi_config::kSsid, wifi_config::kPassword);
    Serial.print("WIFI CONNECTING");

    const unsigned long deadline = millis() + 15000;
    while (WiFi.status() != WL_CONNECTED &&
           static_cast<long>(deadline - millis()) > 0) {
      delay(250);
      Serial.print('.');
    }
    Serial.println();

    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WIFI FAILED");
      WiFi.disconnect();
      return;
    }

    server_.on("/", HTTP_GET,
               [this]() { server_.send_P(200, "text/html", kIndexHtml); });
    server_.on("/command", HTTP_POST, [this]() { receiveCommand(); });
    server_.onNotFound([this]() { server_.send(404, "text/plain", "Not found"); });
    server_.begin();
    connected_ = true;

    Serial.print("WEB READY: http://");
    Serial.println(WiFi.localIP());
  }

  bool poll(Command &command) override {
    if (!connected_) {
      return false;
    }

    server_.handleClient();
    if (commandPending_) {
      commandPending_ = false;
      command = {driveValue_, steeringValue_, true, true};
      return true;
    }
    return false;
  }

private:
  void receiveCommand() {
    if (!server_.hasArg("drive") || !server_.hasArg("steering")) {
      server_.send(400, "text/plain", "Missing drive or steering");
      return;
    }

    const String driveText = server_.arg("drive");
    const String steeringText = server_.arg("steering");
    char *driveEnd = nullptr;
    char *steeringEnd = nullptr;
    const long drive = strtol(driveText.c_str(), &driveEnd, 10);
    const long steering = strtol(steeringText.c_str(), &steeringEnd, 10);
    if (*driveText.c_str() == '\0' || *driveEnd != '\0' ||
        *steeringText.c_str() == '\0' || *steeringEnd != '\0' ||
        drive < -255 || drive > 255 || steering < -100 || steering > 100) {
      server_.send(400, "text/plain", "Invalid command");
      return;
    }

    driveValue_ = static_cast<int>(drive);
    steeringValue_ = static_cast<int>(steering);
    commandPending_ = true;
    server_.send(204);
  }

  WebServer server_{80};
  bool connected_ = false;
  bool commandPending_ = false;
  int driveValue_ = 0;
  int steeringValue_ = 0;
};

DriveMotor drive;
Steering steering;
SerialTransport serialTransport;
WebTransport webTransport;
unsigned long lastDriveCommandMs = 0;
unsigned long lastSteeringCommandMs = 0;

void applyCommand(const Command &command) {
  const unsigned long now = millis();
  if (command.hasDrive) {
    drive.set(command.drive);
    lastDriveCommandMs = now;
  }
  if (command.hasSteering) {
    steering.set(command.steering);
    lastSteeringCommandMs = now;
  }
}

void setup() {
  pinMode(config::kMotorSleepPin, OUTPUT);
  digitalWrite(config::kMotorSleepPin, LOW);

  drive.begin();
  steering.begin();
  serialTransport.begin();
  webTransport.begin();

  lastDriveCommandMs = millis();
  lastSteeringCommandMs = millis();
  drive.enable();
}

void loop() {
  Command command;
  if (serialTransport.poll(command)) {
    applyCommand(command);
  }
  if (webTransport.poll(command)) {
    applyCommand(command);
  }

  if (millis() - lastDriveCommandMs > config::kCommandTimeoutMs) {
    drive.set(0);
  }
  if (millis() - lastSteeringCommandMs > config::kCommandTimeoutMs) {
    steering.set(0);
  }
}
