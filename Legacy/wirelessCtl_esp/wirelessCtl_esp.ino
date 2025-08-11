#include <Bluepad32.h>
// #include <CAN.h>
#include <ESP32-TWAI-CAN.hpp>
#include <driver/twai.h>

#include <math.h>


// Forget用ピン番号
#define forgetPin 16
// バッテリ残量LED用ピン番号
#define batteryStatePin 2

#define CAN_TX 5
#define CAN_RX 4



// --- configuration value ---

// ECANでの通信の有効化
bool isCommCAN = true;

// ECAN通信時のbitの並びを逆向きにするかどうか
//   udlrabxy      yxbarldu
// 0b11000100 -> 0b00100011
bool isBitFlip = false;

// 全部のアナログ系の数値に加える数値
// ECANでは負の数値が送れないので
int canAxisOffset = 128;

// シリアル送信間隔
int updateDuration = 20;

// 詳細情報のシリアル送信の有効化／無効化
bool isVerbose = false;

// スティックの解像度
// つまり速度が停止と何段階か
int axisResolution = 4;

// スティックのデッドゾーン
int axisDeadzone = 120;



// --- internal value ---

CanFrame rxFrame;
ControllerPtr myControllers[BP32_MAX_GAMEPADS];

//                u  d  l  r  a  b  x  y l1 r1 l2 r2 ls rs
int btnState[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
//                  lx ly rx ry
int rawAxiState[] = { 0, 0, 0, 0 };
int axiState[] = { 0, 0, 0, 0 };

int preBtnState[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
int preAxiState[] = { 0, 0, 0, 0 };

bool isFirstCall = true;



// Arduino setup function. Runs in CPU 1
void setup() {
  Serial.begin(115200);


  ESP32Can.setPins(CAN_TX, CAN_RX);
  ESP32Can.setRxQueueSize(5);
  ESP32Can.setTxQueueSize(5);
  ESP32Can.setSpeed(ESP32Can.convertSpeed(1000));

  pinMode(19, OUTPUT);

  // フィルタ：0x220～0x223のうち、bit1無視して0x221/0x223だけ受け入れる
  static twai_filter_config_t f_config = {
    .acceptance_code = (0x220 << 21),  // ACR: 共通部分を左詰め
    .acceptance_mask = (0x2 << 21),    // AMR: bit1を無視（bit1に1を立てる）
    .single_filter = true
  };

  if (ESP32Can.begin(TWAI_SPEED_SIZE, -1, -1, 0xFFFF, 0xFFFF, &f_config)) {
    Serial.println("CAN bus started!");
  } else {
    Serial.println("CAN bus failed!");
  }


  const uint8_t* addr = BP32.localBdAddress();
  if (isVerbose) {
    Serial.printf("Firmware: %s\n", BP32.firmwareVersion());
    Serial.printf("BD Addr: %2X:%2X:%2X:%2X:%2X:%2X\n", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
  }
  // Setup the Bluepad32 callbacks
  BP32.setup(&onConnectedController, &onDisconnectedController);

  // "forgetBluetoothKeys()" should be called when the user performs
  // a "device factory reset", or similar.
  // Calling "forgetBluetoothKeys" in setup() just as an example.
  // Forgetting Bluetooth keys prevents "paired" gamepads to reconnect.
  // But it might also fix some connection / re-connection issues.
  pinMode(forgetPin, INPUT_PULLDOWN);
  pinMode(batteryStatePin, OUTPUT);

  if (digitalRead(forgetPin) == HIGH) {
    BP32.forgetBluetoothKeys();
  }

  // Enables mouse / touchpad support for gamepads that support them.
  // When enabled, controllers like DualSense and DualShock4 generate two connected devices:
  // - First one: the gamepad
  // - Second one, which is a "virtual device", is a mouse.
  // By default, it is disabled.
  BP32.enableVirtualDevice(false);
}


// Arduino loop function. Runs in CPU 1.
void loop() {
  // This call fetches all the controllers' data.
  // Call this function in your main loop.
  bool dataUpdated = BP32.update();
  if (dataUpdated) {
    processControllers();
  }

  /*
  if (ESP32Can.readFrame(rxFrame, 10)) {
    parseCANFrame(rxFrame.data);
  }
  */

  // The main loop must have some kind of "yield to lower priority task" event.
  // Otherwise, the watchdog will get triggered.
  // If your main loop doesn't have one, just add a simple `vTaskDelay(1)`.
  // Detailed info here:
  // https://stackoverflow.com/questions/66278271/task-watchdog-got-triggered-the-tasks-did-not-reset-the-watchdog-in-time

  //     vTaskDelay(1);
  delay(updateDuration);
  // Serial.println(rawAxiState[0]);
}

void parseCANFrame(uint8_t rxPayload[]) {
  uint8_t txPayload[8] = {};
  int len;

  switch (rxPayload[0]) {
    case 7:
      {
        if (rxPayload[1] == 2) {
          // Heartbeat
          uint8_t buf[] = { 7, 0 };
          len = sizeof(buf);
          arrcpy(buf, txPayload, len);

          sendCANFrame(txPayload, len);
        }
        break;
      }

    default:
      {
      }
  }
}

void sendCANFrame(uint8_t payload[], int len) {
  twai_message_t frame = { 0 };
  frame.identifier = 0x222;  // Code:17 Id:1 isSendfromMain:0
  frame.extd = 0;            // standard frame
  frame.data_length_code = len;

  for (int i = 0; i < len; i++) {
    frame.data[i] = payload[i];
  }

  // Accepts both pointers and references
  ESP32Can.writeFrame(frame);  // timeout defaults to 1 ms
}

void arrcpy(uint8_t* src, uint8_t* dest, int len) {
  for (int i = 0; i < len; i++) {
    dest[i] = src[i];
  }
}

uint8_t intArrayToByte(int arr[], int len) {
  if (len > 8) {
    len = 8;
  }

  if (len != 8) {
    for (int i = 0; i < (8 - len); i++) {
      arr[len + i] = 0;
    }
  }

  int byte = 0;

  if (isBitFlip) {
    int arr_flip[8];

    for (int i = 0; i < len; i++) {
      arr_flip[i] = arr[len - 1 - i];
    }

    arr = arr_flip;
  }

  for (int i = 0; i < len; i++) {
    byte += arr[i] * pow(2, i);
  }

  return byte;
}

int axisNormalize(int rawAxis) {
  int normalizedAxis = 0;
  int range = (512 - axisDeadzone) / axisResolution;

  bool isUpper = true;

  for (int j = 0; j < axisResolution; j++) {
    int threshold = axisDeadzone + j * range;
    if (abs(rawAxis) <= threshold) {
      normalizedAxis = j;
      isUpper = false;
      break;
    }
  }

  if (isUpper) {
    normalizedAxis = axisResolution;
  }

  if (rawAxis < 0) {
    normalizedAxis *= -1;
  }

  return normalizedAxis;
}

void ledBlink(int num) {
  for (int i = 0; i < num - 1; i++) {
    digitalWrite(batteryStatePin, HIGH);
    delay(50);
    digitalWrite(batteryStatePin, LOW);
    delay(250);
  }
  digitalWrite(batteryStatePin, HIGH);
  delay(50);
  digitalWrite(batteryStatePin, LOW);
}

void showBatteryState(int batteryState) {
  // バッテリ状態
  // 0 - unknown
  // 1 - battery empty
  // 255 - battery full

  if (batteryState == 0) {
    digitalWrite(batteryStatePin, HIGH);
  }
  if (batteryState <= 60) {
    ledBlink(1);
  } else if (batteryState <= 120) {
    ledBlink(2);
  } else if (batteryState <= 180) {
    ledBlink(3);
  } else {
    ledBlink(4);
  }
}

void dumpController_UART() {
  Serial.print("DATA: ");

  for (int i = 0; i < 14; i++) {
    Serial.print(btnState[i]);
    Serial.print(" ");
  }
  for (int i = 0; i < 4; i++) {
    Serial.print(axiState[i]);
    Serial.print(" ");
  }
  Serial.println(axiState[3]);
}

void dumpController_CAN() {
  digitalWrite(19, HIGH);
  delay(10);
  digitalWrite(19, LOW);

  uint8_t payload[8];

  // index:6 entry:0
  payload[0] = 0xC0;

  int buffer[8];
  for (int i = 0; i < 8; i++) {
    buffer[i] = btnState[i];
  }
  payload[1] = intArrayToByte(buffer, 8);

  int buffer1[6];
  for (int i = 0; i < 6; i++) {
    buffer1[i] = btnState[8 + i];
  }
  payload[2] = intArrayToByte(buffer1, 6);

  for (int i = 0; i < 4; i++) {
    payload[3 + i] = (uint8_t)axiState[i];
  }

  sendCANFrame(payload, 7);
}

bool compareArray(int arr1[], int arr2[], int length) {
  for (int i = 0; i < length; i++) {
    if (arr1[i] != arr2[i]) {
      return false;
    }
  }

  return true;
}

// This callback gets called any time a new gamepad is connected.
// Up to 4 gamepads can be connected at the same time.
void onConnectedController(ControllerPtr ctl) {
  bool foundEmptySlot = false;
  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (myControllers[i] == nullptr) {
      Serial.println("INFO: Controller is connected");
      // Additionally, you can get certain gamepad properties like:
      // Model, VID, PID, BTAddr, flags, etc.
      ControllerProperties properties = ctl->getProperties();
      if (isVerbose) {
        Serial.printf("Controller model: %s, VID=0x%04x, PID=0x%04x\n", ctl->getModelName().c_str(), properties.vendor_id,
                      properties.product_id);
      }
      myControllers[i] = ctl;
      foundEmptySlot = true;
      break;
    }
  }
  if (!foundEmptySlot && isVerbose) {
    Serial.println("INFO: Controller connected, but could not found empty slot");
  }
}

void onDisconnectedController(ControllerPtr ctl) {
  bool foundController = false;

  for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
    if (myControllers[i] == ctl) {
      Serial.printf("INFO: Controller disconnected", i);
      Serial.println();
      myControllers[i] = nullptr;
      foundController = true;
      break;
    }
  }

  if (!foundController && isVerbose) {
    Serial.println("INFO: Controller disconnected, but not found in myControllers");
  }
}

void processGamepad(ControllerPtr ctl) {
  if (isFirstCall) {
    showBatteryState(ctl->battery());
    isFirstCall = false;
  }

  int dpad = ctl->dpad();
  switch (dpad) {
    case 1:
      btnState[0] = 1;

      btnState[1] = 0;
      btnState[2] = 0;
      btnState[3] = 0;
      break;
    case 2:
      btnState[1] = 1;

      btnState[0] = 0;
      btnState[2] = 0;
      btnState[3] = 0;
      break;
    case 4:
      btnState[3] = 1;

      btnState[0] = 0;
      btnState[1] = 0;
      btnState[2] = 0;
      break;
    case 8:
      btnState[2] = 1;

      btnState[0] = 0;
      btnState[1] = 0;
      btnState[3] = 0;
      break;
    case 5:
      btnState[0] = 1;
      btnState[3] = 1;

      btnState[1] = 0;
      btnState[2] = 0;
      break;
    case 6:
      btnState[1] = 1;
      btnState[3] = 1;

      btnState[0] = 0;
      btnState[2] = 0;
      break;
    case 10:
      btnState[1] = 1;
      btnState[2] = 1;

      btnState[0] = 0;
      btnState[3] = 0;
      break;
    case 9:
      btnState[0] = 1;
      btnState[2] = 1;

      btnState[1] = 0;
      btnState[3] = 0;
      break;
    default:
      btnState[0] = 0;
      btnState[1] = 0;
      btnState[2] = 0;
      btnState[3] = 0;
  }

  btnState[4] = ctl->b();
  btnState[5] = ctl->a();
  btnState[6] = ctl->y();
  btnState[7] = ctl->x();
  btnState[8] = ctl->l1();
  btnState[9] = ctl->r1();
  btnState[10] = ctl->l2();
  btnState[11] = ctl->r2();

  btnState[12] = ctl->thumbL();
  btnState[13] = ctl->thumbR();

  rawAxiState[0] = ctl->axisX();
  rawAxiState[1] = ctl->axisY();
  rawAxiState[2] = ctl->axisRX();
  rawAxiState[3] = ctl->axisRY();

  for (int i = 0; i < 4; i++) {
    axiState[i] = axisNormalize(rawAxiState[i]);
    if (isCommCAN) {
      axiState[i] += 128;
    }
  }

  if (!compareArray(btnState, preBtnState, 12) || !compareArray(axiState, preAxiState, 4)) {
    if (isCommCAN) {
      dumpController_CAN();
    } else {
      dumpController_UART();
    }

    for (int i = 0; i < 14; i++) {
      preBtnState[i] = btnState[i];
    }
    for (int i = 0; i < 4; i++) {
      preAxiState[i] = axiState[i];
    }
  }
}

void processControllers() {
  for (auto myController : myControllers) {
    if (myController && myController->isConnected() && myController->hasData()) {
      if (myController->isGamepad()) {
        processGamepad(myController);
      } else {
        Serial.println("ERROR: Unsupported controller");
      }
    }
  }
}
