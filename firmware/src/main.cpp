#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs_flash.h"

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

namespace {

constexpr char kTag[] = "rccar";
constexpr std::uint8_t kSetMovementType = 0x01;
constexpr std::uint8_t kMovementStateType = 0x02;
constexpr std::uint8_t kTelemetryType = 0x03;
constexpr EventBits_t kWifiConnectedBit = BIT0;
constexpr std::size_t kMaxHttpClients = 8;

template <typename T> constexpr T clampValue(T value, T minimum, T maximum) {
  return value < minimum ? minimum : (value > maximum ? maximum : value);
}

constexpr int mapValue(int value, int inputMinimum, int inputMaximum,
                       int outputMinimum, int outputMaximum) {
  return (value - inputMinimum) * (outputMaximum - outputMinimum) /
             (inputMaximum - inputMinimum) +
         outputMinimum;
}

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
    ledc_timer_config_t timer{};
    timer.speed_mode = LEDC_LOW_SPEED_MODE;
    timer.duty_resolution = LEDC_TIMER_8_BIT;
    timer.timer_num = LEDC_TIMER_0;
    timer.freq_hz = config::kMotorPwmFrequencyHz;
    timer.clk_cfg = LEDC_USE_XTAL_CLK;
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    configureChannel(LEDC_CHANNEL_0, config::kMotorAin1Pin);
    configureChannel(LEDC_CHANNEL_1, config::kMotorAin2Pin);
    set(0);
  }

  int set(int command) {
    const int output = scaleMotorCommand(clampValue(command, -255, 255));
    setDuty(LEDC_CHANNEL_0, output < 0 ? -output : 0);
    setDuty(LEDC_CHANNEL_1, output > 0 ? output : 0);
    return output;
  }

private:
  static void configureChannel(ledc_channel_t channel, gpio_num_t pin) {
    ledc_channel_config_t config{};
    config.gpio_num = pin;
    config.speed_mode = LEDC_LOW_SPEED_MODE;
    config.channel = channel;
    config.timer_sel = LEDC_TIMER_0;
    config.duty = 0;
    config.hpoint = 0;
    ESP_ERROR_CHECK(ledc_channel_config(&config));
  }

  static void setDuty(ledc_channel_t channel, std::uint32_t duty) {
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, channel, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, channel));
  }
};

class Steering {
public:
  void begin() {
    ledc_timer_config_t timer{};
    timer.speed_mode = LEDC_LOW_SPEED_MODE;
    timer.duty_resolution = LEDC_TIMER_10_BIT;
    timer.timer_num = LEDC_TIMER_1;
    timer.freq_hz = config::kSteeringFrequencyHz;
    timer.clk_cfg = LEDC_USE_XTAL_CLK;
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    ledc_channel_config_t channel{};
    channel.gpio_num = config::kSteeringPin;
    channel.speed_mode = LEDC_LOW_SPEED_MODE;
    channel.channel = LEDC_CHANNEL_2;
    channel.timer_sel = LEDC_TIMER_1;
    channel.duty = 0;
    channel.hpoint = 0;
    ESP_ERROR_CHECK(ledc_channel_config(&channel));
    set(0);
  }

  int set(int command) {
    command = clampValue(command, -config::kSteeringMaxCommand,
                         config::kSteeringMaxCommand);
    const int angle =
        command < 0
            ? mapValue(command, -100, 0, config::kSteeringMinDegrees,
                       config::kSteeringCenterDegrees)
            : mapValue(command, 0, 100, config::kSteeringCenterDegrees,
                       config::kSteeringMaxDegrees);
    const int pulseUs = mapValue(angle, 0, 180,
                                 config::kSteeringMinimumPulseUs,
                                 config::kSteeringMaximumPulseUs);
    constexpr std::uint32_t kMaximumDuty =
        (1U << config::kSteeringPwmResolutionBits) - 1U;
    constexpr std::uint32_t kPeriodUs =
        1000000U / config::kSteeringFrequencyHz;
    const std::uint32_t duty =
        static_cast<std::uint32_t>(pulseUs) * kMaximumDuty / kPeriodUs;
    ESP_ERROR_CHECK(
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, duty));
    ESP_ERROR_CHECK(
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2));
    return command;
  }
};

struct ControlState {
  int commandedDrive = 0;
  int commandedSteering = 0;
  int appliedDrive = 0;
  int appliedSteering = 0;
  std::uint32_t revision = 0;
  std::int64_t receivedAtUs = 0;
};

struct AsyncFrame {
  httpd_handle_t server;
  int socket;
  std::size_t length;
  std::uint8_t payload[33];
};

DriveMotor drive;
Steering steering;
ControlState controlState;
SemaphoreHandle_t controlMutex = nullptr;
TaskHandle_t controlTaskHandle = nullptr;
TaskHandle_t telemetryTaskHandle = nullptr;
EventGroupHandle_t wifiEvents = nullptr;
httpd_handle_t httpServer = nullptr;

void writeU16(std::uint8_t *destination, std::uint16_t value) {
  destination[0] = static_cast<std::uint8_t>(value >> 8);
  destination[1] = static_cast<std::uint8_t>(value);
}

void writeU32(std::uint8_t *destination, std::uint32_t value) {
  destination[0] = static_cast<std::uint8_t>(value >> 24);
  destination[1] = static_cast<std::uint8_t>(value >> 16);
  destination[2] = static_cast<std::uint8_t>(value >> 8);
  destination[3] = static_cast<std::uint8_t>(value);
}

void writeU64(std::uint8_t *destination, std::uint64_t value) {
  for (int index = 7; index >= 0; --index) {
    destination[index] = static_cast<std::uint8_t>(value);
    value >>= 8;
  }
}

std::int16_t readI16(const std::uint8_t *source) {
  return static_cast<std::int16_t>(
      static_cast<std::uint16_t>(source[0]) << 8 | source[1]);
}

void sendFrameWork(void *argument) {
  auto *pending = static_cast<AsyncFrame *>(argument);
  httpd_ws_frame_t frame{};
  frame.type = HTTPD_WS_TYPE_BINARY;
  frame.payload = pending->payload;
  frame.len = pending->length;
  if (httpd_ws_send_frame_async(pending->server, pending->socket, &frame) !=
      ESP_OK) {
    ESP_LOGD(kTag, "WebSocket send failed for socket %d", pending->socket);
  }
  delete pending;
}

void queueFrame(int socket, const std::uint8_t *payload, std::size_t length) {
  if (httpServer == nullptr || length > sizeof(AsyncFrame::payload)) {
    return;
  }
  auto *pending = new (std::nothrow) AsyncFrame{};
  if (pending == nullptr) {
    return;
  }
  pending->server = httpServer;
  pending->socket = socket;
  pending->length = length;
  std::memcpy(pending->payload, payload, length);
  if (httpd_queue_work(httpServer, sendFrameWork, pending) != ESP_OK) {
    delete pending;
  }
}

void broadcast(const std::uint8_t *payload, std::size_t length) {
  if (httpServer == nullptr) {
    return;
  }
  int sockets[kMaxHttpClients]{};
  std::size_t count = kMaxHttpClients;
  if (httpd_get_client_list(httpServer, &count, sockets) != ESP_OK) {
    return;
  }
  for (std::size_t index = 0; index < count; ++index) {
    if (httpd_ws_get_fd_info(httpServer, sockets[index]) ==
        HTTPD_WS_CLIENT_WEBSOCKET) {
      queueFrame(sockets[index], payload, length);
    }
  }
}

void encodeMovementState(std::uint8_t (&payload)[9],
                         const ControlState &snapshot) {
  payload[0] = kMovementStateType;
  writeU16(payload + 1, static_cast<std::uint16_t>(snapshot.commandedDrive));
  writeU16(payload + 3,
           static_cast<std::uint16_t>(snapshot.commandedSteering));
  writeU16(payload + 5, static_cast<std::uint16_t>(snapshot.appliedDrive));
  writeU16(payload + 7,
           static_cast<std::uint16_t>(snapshot.appliedSteering));
}

void broadcastMovementState(const ControlState &snapshot) {
  std::uint8_t payload[9];
  encodeMovementState(payload, snapshot);
  broadcast(payload, sizeof(payload));
}

void encodeTelemetry(std::uint8_t (&payload)[33]) {
  wifi_ap_record_t accessPoint{};
  const bool hasAccessPoint = esp_wifi_sta_get_ap_info(&accessPoint) == ESP_OK;
  const std::size_t psramTotal = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
  const std::int32_t psramFree =
      psramTotal == 0
          ? -1
          : static_cast<std::int32_t>(
                heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  const std::int32_t psramLargest =
      psramTotal == 0
          ? -1
          : static_cast<std::int32_t>(
                heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));

  payload[0] = kTelemetryType;
  writeU64(payload + 1,
           static_cast<std::uint64_t>(esp_timer_get_time() / 1000));
  writeU16(payload + 9, static_cast<std::uint16_t>(
                           hasAccessPoint ? accessPoint.rssi : 0));
  writeU32(payload + 11, static_cast<std::uint32_t>(
                             heap_caps_get_free_size(MALLOC_CAP_INTERNAL)));
  writeU32(payload + 15,
           static_cast<std::uint32_t>(
               heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
  writeU32(payload + 19, static_cast<std::uint32_t>(psramFree));
  writeU32(payload + 23, static_cast<std::uint32_t>(psramLargest));
  payload[27] = xEventGroupGetBits(wifiEvents) & kWifiConnectedBit ? 0x01 : 0;
  payload[28] = 0;
  writeU32(payload + 29, 0);
}

void broadcastTelemetry() {
  std::uint8_t payload[33];
  encodeTelemetry(payload);
  broadcast(payload, sizeof(payload));
}

void queueCurrentState(int socket) {
  ControlState snapshot;
  xSemaphoreTake(controlMutex, portMAX_DELAY);
  snapshot = controlState;
  xSemaphoreGive(controlMutex);
  std::uint8_t movement[9];
  std::uint8_t telemetry[33];
  encodeMovementState(movement, snapshot);
  encodeTelemetry(telemetry);
  queueFrame(socket, movement, sizeof(movement));
  queueFrame(socket, telemetry, sizeof(telemetry));
}

void submitMovement(int driveValue, int steeringValue) {
  xSemaphoreTake(controlMutex, portMAX_DELAY);
  controlState.commandedDrive = driveValue;
  controlState.commandedSteering = steeringValue;
  controlState.receivedAtUs = esp_timer_get_time();
  ++controlState.revision;
  xSemaphoreGive(controlMutex);
  xTaskNotifyGive(controlTaskHandle);
  xTaskNotifyGive(telemetryTaskHandle);
}

void controlTask(void *) {
  std::uint32_t appliedRevision = 0;
  while (true) {
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10));

    ControlState snapshot;
    xSemaphoreTake(controlMutex, portMAX_DELAY);
    const std::int64_t now = esp_timer_get_time();
    if (now - controlState.receivedAtUs >
            static_cast<std::int64_t>(config::kCommandTimeoutMs) * 1000 &&
        (controlState.commandedDrive != 0 ||
         controlState.commandedSteering != 0)) {
      controlState.commandedDrive = 0;
      controlState.commandedSteering = 0;
      ++controlState.revision;
    }
    snapshot = controlState;
    xSemaphoreGive(controlMutex);

    if (snapshot.revision == appliedRevision) {
      continue;
    }

    const int appliedDrive = drive.set(snapshot.commandedDrive);
    const int appliedSteering = steering.set(snapshot.commandedSteering);
    snapshot.appliedDrive = appliedDrive;
    snapshot.appliedSteering = appliedSteering;
    xSemaphoreTake(controlMutex, portMAX_DELAY);
    controlState.appliedDrive = appliedDrive;
    controlState.appliedSteering = appliedSteering;
    appliedRevision = snapshot.revision;
    xSemaphoreGive(controlMutex);
    broadcastMovementState(snapshot);
  }
}

void telemetryTask(void *) {
  while (true) {
    broadcastTelemetry();
    xSemaphoreTake(controlMutex, portMAX_DELAY);
    const bool active =
        esp_timer_get_time() - controlState.receivedAtUs <=
        static_cast<std::int64_t>(config::kCommandTimeoutMs) * 1000;
    xSemaphoreGive(controlMutex);
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(active ? 500 : 30000));
  }
}

esp_err_t rootHandler(httpd_req_t *request) {
  httpd_resp_set_type(request, "text/html");
  httpd_resp_set_hdr(request, "Cache-Control", "no-store");
  return httpd_resp_send(request, kIndexHtml, HTTPD_RESP_USE_STRLEN);
}

esp_err_t websocketHandler(httpd_req_t *request) {
  httpd_ws_frame_t frame{};
  if (httpd_ws_recv_frame(request, &frame, 0) != ESP_OK) {
    return ESP_FAIL;
  }
  if (frame.type == HTTPD_WS_TYPE_PONG) {
    std::uint8_t pong[125];
    if (frame.len > sizeof(pong)) {
      return ESP_FAIL;
    }
    frame.payload = pong;
    return httpd_ws_recv_frame(request, &frame, sizeof(pong));
  }
  if (frame.len != 5) {
    return ESP_FAIL;
  }
  std::uint8_t payload[5];
  frame.payload = payload;
  if (httpd_ws_recv_frame(request, &frame, sizeof(payload)) != ESP_OK ||
      frame.type != HTTPD_WS_TYPE_BINARY || payload[0] != kSetMovementType) {
    return ESP_FAIL;
  }

  const int driveValue = readI16(payload + 1);
  const int steeringValue = readI16(payload + 3);
  if (driveValue < -255 || driveValue > 255 || steeringValue < -100 ||
      steeringValue > 100) {
    return ESP_FAIL;
  }
  submitMovement(driveValue, steeringValue);
  return ESP_OK;
}

esp_err_t websocketConnected(httpd_req_t *request) {
  queueCurrentState(httpd_req_to_sockfd(request));
  return ESP_OK;
}

void startHttpServer() {
  httpd_config_t serverConfig = HTTPD_DEFAULT_CONFIG();
  serverConfig.max_uri_handlers = 2;
  serverConfig.lru_purge_enable = true;
  serverConfig.stack_size = 6144;
  ESP_ERROR_CHECK(httpd_start(&httpServer, &serverConfig));

  httpd_uri_t root{};
  root.uri = "/";
  root.method = HTTP_GET;
  root.handler = rootHandler;
  ESP_ERROR_CHECK(httpd_register_uri_handler(httpServer, &root));

  httpd_uri_t websocket{};
  websocket.uri = "/ws/v1/control";
  websocket.method = HTTP_GET;
  websocket.handler = websocketHandler;
  websocket.is_websocket = true;
  websocket.ws_post_handshake_cb = websocketConnected;
  ESP_ERROR_CHECK(httpd_register_uri_handler(httpServer, &websocket));
}

void wifiEventHandler(void *, esp_event_base_t eventBase, std::int32_t eventId,
                      void *eventData) {
  if (eventBase == WIFI_EVENT && eventId == WIFI_EVENT_STA_START) {
    ESP_ERROR_CHECK(esp_wifi_connect());
  } else if (eventBase == WIFI_EVENT &&
             eventId == WIFI_EVENT_STA_DISCONNECTED) {
    const auto *event =
        static_cast<wifi_event_sta_disconnected_t *>(eventData);
    xEventGroupClearBits(wifiEvents, kWifiConnectedBit);
    ESP_LOGW(kTag, "Wi-Fi disconnected (reason=%u, RSSI=%d); reconnecting",
             event->reason, event->rssi);
    esp_wifi_connect();
  } else if (eventBase == IP_EVENT && eventId == IP_EVENT_STA_GOT_IP) {
    const auto *event = static_cast<ip_event_got_ip_t *>(eventData);
    xEventGroupSetBits(wifiEvents, kWifiConnectedBit);
    ESP_LOGI(kTag, "WEB READY: http://" IPSTR, IP2STR(&event->ip_info.ip));
    broadcastTelemetry();
  }
}

void startWifi() {
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_t *stationNetif = esp_netif_create_default_wifi_sta();
  ESP_ERROR_CHECK(stationNetif == nullptr ? ESP_FAIL : ESP_OK);
  ESP_ERROR_CHECK(esp_netif_set_hostname(stationNetif, "rccar"));

  wifi_init_config_t initialization = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&initialization));
  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                             wifiEventHandler, nullptr));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                             wifiEventHandler, nullptr));

  wifi_config_t station{};
  std::strncpy(reinterpret_cast<char *>(station.sta.ssid), wifi_config::kSsid,
               sizeof(station.sta.ssid) - 1);
  std::strncpy(reinterpret_cast<char *>(station.sta.password),
               wifi_config::kPassword, sizeof(station.sta.password) - 1);
  station.sta.threshold.authmode = WIFI_AUTH_OPEN;
  station.sta.pmf_cfg.capable = true;
  station.sta.pmf_cfg.required = false;
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &station));
  ESP_LOGI(kTag, "Connecting to Wi-Fi SSID \"%s\"", wifi_config::kSsid);
  ESP_ERROR_CHECK(esp_wifi_start());
}

void initializeActuators() {
  gpio_config_t sleepPin{};
  sleepPin.pin_bit_mask = 1ULL << config::kMotorSleepPin;
  sleepPin.mode = GPIO_MODE_OUTPUT;
  ESP_ERROR_CHECK(gpio_config(&sleepPin));
  ESP_ERROR_CHECK(gpio_set_level(config::kMotorSleepPin, 0));

  drive.begin();
  steering.begin();
  ESP_ERROR_CHECK(gpio_set_level(config::kMotorSleepPin, 1));
  vTaskDelay(pdMS_TO_TICKS(1));
}

} // namespace

extern "C" void app_main() {
  initializeActuators();
  controlMutex = xSemaphoreCreateMutex();
  wifiEvents = xEventGroupCreate();
  ESP_ERROR_CHECK(controlMutex == nullptr || wifiEvents == nullptr ? ESP_FAIL
                                                                  : ESP_OK);
  controlState.receivedAtUs = esp_timer_get_time();

  ESP_ERROR_CHECK(xTaskCreate(controlTask, "motor_control", 4096, nullptr, 10,
                              &controlTaskHandle) == pdPASS
                      ? ESP_OK
                      : ESP_FAIL);
  ESP_ERROR_CHECK(xTaskCreate(telemetryTask, "telemetry", 3072, nullptr, 4,
                              &telemetryTaskHandle) == pdPASS
                      ? ESP_OK
                      : ESP_FAIL);

  esp_err_t nvsResult = nvs_flash_init();
  if (nvsResult == ESP_ERR_NVS_NO_FREE_PAGES ||
      nvsResult == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    nvsResult = nvs_flash_init();
  }
  ESP_ERROR_CHECK(nvsResult);

  if (wifi_config::kSsid[0] == '\0') {
    ESP_LOGW(kTag, "Wi-Fi disabled: create include/wifi_config.h");
    return;
  }

  startWifi();
  xEventGroupWaitBits(wifiEvents, kWifiConnectedBit, pdFALSE, pdTRUE,
                      portMAX_DELAY);
  startHttpServer();
}
