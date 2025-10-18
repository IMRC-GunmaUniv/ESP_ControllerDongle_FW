#include <Bluepad32.h>

// ----- wirelessCtl.inoのプロトタイプ宣言 -----
void cont_init();
void cont_updateController();
int cont_getBtnState(String key);
void cont_forgetController();
int cont_getBatteryState(int batteryState);
int _cont_axisNormalize(int rawAxis);
bool _cont_compareArray(int arr1[], int arr2[], int length);
void _cont_onConnectedController(ControllerPtr ctl);
void _cont_onDisconnectedController(ControllerPtr ctl);
void _cont_processGamepad(ControllerPtr ctl);
void _cont_processControllers();
// ----- wirelessCtl.inoのプロトタイプ宣言 終わり -----

void setup()
{
    cont_init();
}

void loop()
{
    cont_updateController();
}