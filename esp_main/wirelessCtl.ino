// wirelessCtl.ino standalone v1.1

// 重要
// 変数や関数の名前の最初の文字が"_"になっているものは触らない、アクセスしないことをお勧めします。
// 実装していくうえで、このプログラムについての疑問点やバグとかを見つけたら、#プログラム系で連絡してください。


ControllerPtr _cont_myControllers[BP32_MAX_GAMEPADS];


// ----- 変数 -----


// スティックの解像度
// つまり速度が停止と何段階か
const int cont_axisResolution = 4;

// スティックのデッドゾーン(生の値での)
const int cont_axisDeadzone = 120;

// 以下のcont_xxxState配列でコントローラーのボタンやスティックの状態を参照できる
// が、特別な理由がない限りgetBtnState関数で取得するのがおススメ(可読性の面から)

// ボタン 押されてれば1、押されてなければ0が入る
//                     UP DO LE RI A  B  X  Y L1 R1 L2 R2 LS RS
int cont_btnState[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

// スティック
//                        LX LY RX RY
// rawは生の値を格納(多分-512 ~ 512)
int cont_rawAxiState[] = {0, 0, 0, 0};
// こっちはrawをcont_axisResolutionで正規化したもの
// -cont_axisResolution ~ cont_axisResolutionをとる
int cont_axiState[] = {0, 0, 0, 0};

// 直前のボタン、スティックの状態を格納
// トグル処理とかしたかったら使えるかも
int cont_preBtnState[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int cont_preAxiState[] = {0, 0, 0, 0};

// バッテリ残量(詳しい内容はgetBatteryState内に)
// getBatteryStateで取得もできる
int cont_batteryLevel;


// ----- 関数 -----


void cont_init()
{
    // 無線コントローラプログラムの初期化
    // void setupで必ず呼び出してね
    const uint8_t *addr = BP32.localBdAddress();

    BP32.setup(&_cont_onConnectedController, &_cont_onDisconnectedController);

    BP32.enableVirtualDevice(false);
}

void cont_updateController()
{
    // 無線コントローラのメイン処理
    // コントローラと通信、状態取得、結果を変数に格納したりする
    // void loopで高頻度で必ず呼び出してね(特大delayとかダメ)
    bool dataUpdated = BP32.update();
    if (dataUpdated)
    {
        _cont_processControllers();
    }
}

int cont_getBtnState(String key)
{
    // cont_getBtnState("A")で、〇ボタンのon/offが返ってくる
    // 押されてれば1、押されてなければ0

    // int ue = cont_getBtnState("UP");とかで十字の上ボタンが押されてるか返ってくる
    // 引数は下のkeyMapと一致するように
    
    String keyMap[] = {"UP", "DOWN", "LEFT", "RIGHT", "A", "B", "X", "Y", "L1", "R1", "L2", "R2", "LS", "RS"};

    for (int i = 0; i < 14; i++)
    {
        if (keyMap[i] == key)
        {
            return cont_btnState[i];
        }
    }

    return 0;
}

int cont_getAxiState(String key)
{
    // cont_getAxiState("LY")で、スティックの軸の正規化された値が返ってくる
    // Xは右が+、Yは下が+(Y軸だけキモい)

    // getBtnStateと同じように、引数はkeyMapと一致するように

    String keyMap[] = {"LX", "LY", "RX", "RY"};

    for (int i = 0; i < 4; i++)
    {
        if (keyMap[i] == key)
        {
            return cont_axiState[i];
        }
    }
}

void cont_forgetController()
{
    // ペアリングしたコントローラーをESPのメモリから消去する
    // またペアリングしなおせば使える
    BP32.forgetBluetoothKeys();
}

int cont_getBatteryState(int batteryState)
{
    // コントローラのバッテリ状態を返す
    // 0 - バッテリ状態不明
    // 1 - バッテリ0%
    // 255 - バッテリ100%

    return cont_batteryLevel;
}


// ----- 以下、ヘルパー関数(読める人以外いじるな触るな呼び出すな) -----


int _cont_axisNormalize(int rawAxis)
{
    // スティックの生の値から、cont_axisResolutionで正規化する

    int normalizedAxis = 0;
    int range = (512 - cont_axisDeadzone) / cont_axisResolution;

    bool isUpper = true;

    for (int j = 0; j < cont_axisResolution; j++)
    {
        int threshold = cont_axisDeadzone + j * range;
        if (abs(rawAxis) <= threshold)
        {
            normalizedAxis = j;
            isUpper = false;
            break;
        }
    }

    if (isUpper)
    {
        normalizedAxis = cont_axisResolution;
    }

    if (rawAxis < 0)
    {
        normalizedAxis *= -1;
    }

    return normalizedAxis;
}

bool _cont_compareArray(int arr1[], int arr2[], int length)
{
    // 二つの配列が同じものか判定する
    // 比べたい配列と配列の長さを引数に取る
    // 同じだったらtrue、違うならfalseが返ってくる

    for (int i = 0; i < length; i++)
    {
        if (arr1[i] != arr2[i])
        {
            return false;
        }
    }

    return true;
}

void _cont_onConnectedController(ControllerPtr ctl)
{
    // コントローラと接続したときに一度だけ呼び出される関数
    // ここになんか仕込んでもいいかもね

    bool foundEmptySlot = false;
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++)
    {
        if (_cont_myControllers[i] == nullptr)
        {
            ControllerProperties properties = ctl->getProperties();
            _cont_myControllers[i] = ctl;
            foundEmptySlot = true;
            break;
        }
    }
}

void _cont_onDisconnectedController(ControllerPtr ctl)
{
    // コントローラが切断された時に一度だけ実行される関数
    // 安全機能として、切断されたら強制的に止まるようにしてもいいかもね

    bool foundController = false;

    for (int i = 0; i < BP32_MAX_GAMEPADS; i++)
    {
        if (_cont_myControllers[i] == ctl)
        {
            _cont_myControllers[i] = nullptr;
            foundController = true;
            break;
        }
    }
}

void _cont_processGamepad(ControllerPtr ctl)
{
    // コントローラの状態を格納する構造体から、配列に移す関数

    cont_batteryLevel = ctl->battery();

    int dpad = ctl->dpad();
    switch (dpad)
    {
    case 1:
        cont_btnState[0] = 1;

        cont_btnState[1] = 0;
        cont_btnState[2] = 0;
        cont_btnState[3] = 0;
        break;
    case 2:
        cont_btnState[1] = 1;

        cont_btnState[0] = 0;
        cont_btnState[2] = 0;
        cont_btnState[3] = 0;
        break;
    case 4:
        cont_btnState[3] = 1;

        cont_btnState[0] = 0;
        cont_btnState[1] = 0;
        cont_btnState[2] = 0;
        break;
    case 8:
        cont_btnState[2] = 1;

        cont_btnState[0] = 0;
        cont_btnState[1] = 0;
        cont_btnState[3] = 0;
        break;
    case 5:
        cont_btnState[0] = 1;
        cont_btnState[3] = 1;

        cont_btnState[1] = 0;
        cont_btnState[2] = 0;
        break;
    case 6:
        cont_btnState[1] = 1;
        cont_btnState[3] = 1;

        cont_btnState[0] = 0;
        cont_btnState[2] = 0;
        break;
    case 10:
        cont_btnState[1] = 1;
        cont_btnState[2] = 1;

        cont_btnState[0] = 0;
        cont_btnState[3] = 0;
        break;
    case 9:
        cont_btnState[0] = 1;
        cont_btnState[2] = 1;

        cont_btnState[1] = 0;
        cont_btnState[3] = 0;
        break;
    default:
        cont_btnState[0] = 0;
        cont_btnState[1] = 0;
        cont_btnState[2] = 0;
        cont_btnState[3] = 0;
    }

    cont_btnState[4] = ctl->b();
    cont_btnState[5] = ctl->a();
    cont_btnState[6] = ctl->y();
    cont_btnState[7] = ctl->x();
    cont_btnState[8] = ctl->l1();
    cont_btnState[9] = ctl->r1();
    cont_btnState[10] = ctl->l2();
    cont_btnState[11] = ctl->r2();

    cont_btnState[12] = ctl->thumbL();
    cont_btnState[13] = ctl->thumbR();

    cont_rawAxiState[0] = ctl->axisX();
    cont_rawAxiState[1] = ctl->axisY();
    cont_rawAxiState[2] = ctl->axisRX();
    cont_rawAxiState[3] = ctl->axisRY();

    for (int i = 0; i < 4; i++)
    {
        cont_axiState[i] = _cont_axisNormalize(cont_rawAxiState[i]);
    }

    if (!_cont_compareArray(cont_btnState, cont_preBtnState, 12) || !_cont_compareArray(cont_axiState, cont_preAxiState, 4))
    {
        for (int i = 0; i < 14; i++)
        {
            cont_preBtnState[i] = cont_btnState[i];
        }
        for (int i = 0; i < 4; i++)
        {
            cont_preAxiState[i] = cont_axiState[i];
        }
    }
}

void _cont_processControllers()
{
    // よくわからん

    for (auto myController : _cont_myControllers)
    {
        if (myController && myController->isConnected() && myController->hasData())
        {
            if (myController->isGamepad())
            {
                _cont_processGamepad(myController);
            }
        }
    }
}
