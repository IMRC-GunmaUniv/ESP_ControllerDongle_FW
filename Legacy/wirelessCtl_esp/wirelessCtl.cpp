#include <Bluepad32.h>

ControllerPtr myControllers[BP32_MAX_GAMEPADS];

// スティックの解像度
// つまり速度が停止と何段階か
int axisResolution = 4;
int axisDeadzone = 120;

//                u  d  l  r  a  b  x  y l1 r1 l2 r2 ls rs
int btnState[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
//                  lx ly rx ry
int rawAxiState[] = {0, 0, 0, 0};
int axiState[]    = {0, 0, 0, 0};

int preBtnState[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int preAxiState[] = {0, 0, 0, 0};


int batteryLevel = 0;

int axisNormalize(int rawAxis){
  int normalizedAxis = 0;
  int range = (512 - axisDeadzone) / axisResolution;

  bool isUpper = true;

  for(int j = 0; j < axisResolution; j++){
    int threshold = axisDeadzone + j * range;
    if(abs(rawAxis) <= threshold){
      normalizedAxis = j;
      isUpper = false;
      break;
    }
  }

  if(isUpper){
    normalizedAxis = axisResolution;
  }

  if(rawAxis < 0){
    normalizedAxis *= -1;
  }

  return normalizedAxis;
}

int showBatteryState(int batteryState){  
  // バッテリ状態
  // 0 - unknown
  // 1 - battery empty
  // 255 - battery full

  return batteryLevel;
}

bool compareArray(int arr1[], int arr2[], int length){
  for(int i = 0; i < length; i++){
    if(arr1[i] != arr2[i]){
      return false;
    }
  }

  return true;
}

void onConnectedController(ControllerPtr ctl) {
    bool foundEmptySlot = false;
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (myControllers[i] == nullptr) {
            ControllerProperties properties = ctl->getProperties();
            myControllers[i] = ctl;
            foundEmptySlot = true;
            break;
        }
    }
}

void onDisconnectedController(ControllerPtr ctl) {
    bool foundController = false;

    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (myControllers[i] == ctl) {
            myControllers[i] = nullptr;
            foundController = true;
            break;
        }
    }
}

void processGamepad(ControllerPtr ctl) {
  batteryLevel = ctl->battery();
  
  int dpad = ctl->dpad();
  switch(dpad){
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

  for(int i = 0; i < 4; i++){
    axiState[i] = axisNormalize(rawAxiState[i]);
  }

  if(!compareArray(btnState, preBtnState, 12) || !compareArray(axiState, preAxiState, 4)){
    for(int i = 0; i < 14; i++){
      preBtnState[i] = btnState[i];
    }
    for(int i = 0; i < 4; i++){
      preAxiState[i] = axiState[i];
    }
  }
}

void processControllers() {
    for (auto myController : myControllers) {
        if (myController && myController->isConnected() && myController->hasData()) {
            if (myController->isGamepad()) {
                processGamepad(myController);
            }
        }
    }
}

void forgetController(){
  BP32.forgetBluetoothKeys();
}

void init() {
    const uint8_t* addr = BP32.localBdAddress();

    BP32.setup(&onConnectedController, &onDisconnectedController);

    BP32.enableVirtualDevice(false);
}

void updateController() {
    bool dataUpdated = BP32.update();
    if (dataUpdated){
      processControllers();
    }
}
