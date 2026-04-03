#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128         // 屏幕宽度（像素）
#define SCREEN_HEIGHT 64         // 屏幕高度（像素）
#define OLED_RESET -1            // 复位引脚（-1 表示与开发板公用复位）
#define SCREEN_ADDRESS 0x3C      // I2C 地址


// LCD 引脚定义
#define I2C_SDA 21                // SDA 引脚
#define I2C_SCL 22                // SCL 引脚

// 四角微动开关引脚定义
#define BTN_PIN 25                // BTN 引脚

// NLSF 引脚定义
#define NLSF_DATA_PIN 23          // SI  （串行数据输入）
#define NLSF_LATCH_PIN 5          // RCK （锁存时钟）
#define NLSF_CLOCK_PIN 18         // SCK （移位时钟）



// 595 输出位定义
// LED1 (RGB LED #1)
#define LED1_RED_BIT 2            // Q0
#define LED1_GREEN_BIT 1          // Q1
#define LED1_BLUE_BIT 0           // Q2
// LED2 (RGB LED #2)
#define LED2_RED_BIT 5            // Q3
#define LED2_GREEN_BIT 4          // Q4
#define LED2_BLUE_BIT 3           // Q5

// RED LED 定义
#define LED4_PIN 19               // RED4 LED 引脚定义



// RGB LED 类型配置
// 共阴极(Common Cathode): HIGH=亮, LOW=灭 → INVERT=false
// 共阳极(Common Anode):   LOW=亮, HIGH=灭 → INVERT=true
#define RGB_INVERT true

// 595 当前输出状态（8位）
byte shiftRegState = 0x00;

// RGB LED 动画相关
enum RgbMode {
  RGB_IDLE,                       // 无动画
  RGB_RED_STEADY,                 // 红色常亮
  RGB_BLUE_STEADY,                // 蓝色常亮
  RGB_GREEN_STEADY,               // 绿色常亮
  RGB_COLOR_CHANGE,               // 每秒变色
  RGB_RAINBOW,                    // 彩虹循环
  RGB_BREATHING,                  // 呼吸灯效果
  RGB_COLOR_WHEEL,                // 色轮
};

// LED 模式
RgbMode currentRgbMode = RGB_IDLE;

// 模式定义
enum LightMode
{
  MODE_OFF,    // 关灯
  MODE_ON,     // 常亮
  MODE_BLINK,  // 闪烁
  MODE_BREATHE // 呼吸灯
};
LightMode currentLedMode = MODE_OFF;

unsigned long rgbLastUpdate = 0;
int rgbColorIndex = 0;
int rgbBreathStep = 0;
bool rgbBreathDir = true; // true：渐亮 false: 渐暗
unsigned long rgbBreathLastUpdate = 0;

const byte COLOR_TABLE[][3] = {
  {1, 0, 0}, // 红
  {0, 1, 0}, // 绿
  {0, 0, 1}, // 蓝
  {1, 1, 0}, // 黄
  {0, 1, 1}, // 青
  {1, 0, 1}, // 紫
  {1, 1, 1}, // 白
};

const int COLOR_COUNT = sizeof(COLOR_TABLE) / sizeof(COLOR_TABLE[0]);

// 摇杆定义
#define VERT_PIN 12                // 纵轴输出
#define HORZ_PIN 14                // 水平输出
#define SEL_PIN 27                 // 中间按钮

#define JOY_CENTER 2048             // 摇杆中间值 
#define JOY_THRESHOLD 1800          // 偏移超过此值才认为有方向输入
#define JOY_REPEAT_FIRST 300        // 首次触发后的等待时间（ms）
#define JOY_REPEAT_NEXT 150         // 连续触发间隔（ms）

// 创建屏幕对象，之后所有对屏幕的操作都通过display.xx() 来调用
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

int counter = 0;                   // 计数器，用于演示动态数据
unsigned long lastUpdateTime = 0;  // 上次更新屏幕的时间
int demoStep = 0;                  // 当前演示阶段
int lastDemoStep = -1;             // 记录上次状态

// 按键消抖相关变量
bool lastButtonState = HIGH;             // 上一次读到的按键状态（HIGH=未按下）
bool currentButtonState = HIGH;          // 当前确认的按键状态
unsigned long lastDebounceTime = 0;      // 上一次按键状态变化的时间
const unsigned long DEBOUNCE_DELAY = 50; // 消抖延时：50毫秒


// 记录是否正在滚动
bool isScroll = false;

// 摇杆方向枚举
enum JoyDirection {
  JOY_NONE,
  JOY_UP,
  JOY_DOWN,
  JOY_LEFT,
  JOY_RIGHT,
  JOY_PRESS
};



// 菜单项结构体
struct MenuItem {
  const char* label;                      // 菜单文字
  MenuItem* subMenu;                      // 子菜单数组指针（NULL=无子菜单）
  int subMenuCount;                       // 子菜单项数量
  void (*action)(const char* label);      // 叶子节点的回调函数
};

// 前向声明： 菜单动作回调函数
void onTextDemo(const char* label);
void onShapesDemo(const char* label);
void onProgressBarDemo(const char* label);
void onScrollDemo(const char* label);
void onLedAction(const char* label);
void onRgbLedDemo(const char* label);
void onButtonDemo(const char* label);


// 子菜单定义：LED Demo 的二级菜单
MenuItem ledSubMenu[] = {
  {"Steady On", NULL, 0, onLedAction},
  {"Blinking", NULL, 0, onLedAction},
  {"Breathing", NULL, 0, onLedAction},
  {"Off", NULL, 0, onLedAction},
};

const int ledSubMenuCount = sizeof(ledSubMenu) / sizeof(ledSubMenu[0]);

// 子菜单定义：RBG LED Demo 的二级菜单
MenuItem rgbLedSubMenu[] = {
  {"Red steady on", NULL, 0, onRgbLedDemo},
  {"Blue steady on", NULL, 0, onRgbLedDemo},
  {"Green steady on", NULL, 0, onRgbLedDemo},
  {"Change color every 1s", NULL, 0, onRgbLedDemo},
  {"Rainbow cycle", NULL, 0, onRgbLedDemo},
  {"Breathing effect", NULL, 0, onRgbLedDemo},
  {"Color wheel", NULL, 0, onRgbLedDemo},
  {"All OFF", NULL, 0, onRgbLedDemo},
};

const int rgbLedSubMenuCount = sizeof(rgbLedSubMenu) / sizeof(rgbLedSubMenu[0]);

// 主菜单定义
MenuItem mainMenu[] = {
  {"Text Demo-Different font size examples", NULL, 0, onTextDemo},
  {"Shapes Demo-Draw triangles/rectangles/circles", NULL, 0, onShapesDemo},
  {"ProgressBar Demo", NULL, 0, onProgressBarDemo},
  {"Scroll Demo", NULL, 0, onScrollDemo },
  {"LED Demo-Constant / Blinking / Breathing light", ledSubMenu, ledSubMenuCount, NULL},
  {"RGBLED Demo", rgbLedSubMenu, rgbLedSubMenuCount, NULL},
  {"BUTTON Demo", NULL, 0, onButtonDemo},
};

const int mainMenuCount = sizeof(mainMenu) / sizeof(mainMenu[0]);


// 菜单导航状态（支持多级嵌套）
#define MAX_MENU_DEPTH 5

struct MenuLevel {
  MenuItem* items;
  int count;
  int selectedIndex;
  int scrollOffset;
};

MenuLevel menuStack[MAX_MENU_DEPTH];
int menuDepth = 0;

// 文字水平滚动相关
int textScrollOffset = 0;
unsigned long lastTextScrollTime = 0;
const unsigned long TEXT_SCROLL_INTERVAL = 100;
const int TEXT_SCROLL_PAUSE_COUNT = 15;
int textScrollPause = 0;


// 显示参数
const int FONT_WIDTH = 6;
const int FONT_HEIGHT = 8;
const int LINE_HEIGHT = 12;
const int TITLE_HEIGHT = 14;
const int ARROW_WIDTH = 8;
const int TEXT_AREA_X = ARROW_WIDTH + 2;
const int TEXT_AREA_WIDTH = SCREEN_WIDTH - TEXT_AREA_X - 2;
const int VISIBLE_LINES = (SCREEN_HEIGHT - TITLE_HEIGHT) / LINE_HEIGHT;



// 摇杆状态
JoyDirection lastJoyDir = JOY_NONE;
unsigned long joyHoldStart = 0;
unsigned long lastJoyRepeat = 0;
bool joyFirstTriggered = false;

bool lastButtonState_sel = HIGH;
bool currentButtonState_sel = HIGH;
unsigned long lastDebounceTime_sel = 0;
const unsigned long DEBOUNCE_DELAY_SEL = 50;



// 是否在“动作执行中”状态
bool inActionMode = false;
const char* actionLabel = NULL;



// PWM 参数，用于控制LED4
const int PWM_FREQ = 5000;
const int PWM_RESOLUTION = 8;

// 闪烁模式相关变量
unsigned long lastBlinkTime = 0;             // 上一次 LED 切换亮灭的时间
const unsigned long BLINK_INTERVAL = 300;    // 闪烁间隔：300毫秒
bool blinkState = false;                     // 当前闪烁状态


// 呼吸灯模式相关变量
int breathBrighness = 0;                   // 当前亮度值（0~255）
int breathDirection = 5;                   // 亮度变化步长和方向
unsigned long lastBreatheTime = 0;         // 上一次更新亮度的时间
const unsigned long BREATHE_INTERVAL = 15; // 亮度更新间隔：15毫秒


// 函数前向声明
JoyDirection readJoystick();
void handleJoystickInput(JoyDirection dir);
void enterSelected();
void goBack();
void drawMenu();
void drawStaticText(const char* text, int x, int y);
void drawScrollingText(const char* text, int x, int y, bool isSelected);
void updateTextScroll();
void resetTextScroll();
void drawScrollBar(int offset, int totalItems, int visibleItems);
void showActionScreen(const char* label);


// 595+RGB 函数前向声明
void shiftOut595(byte data);
void setShiftBit(int bit, bool value);
void clearShiftReg();
void setLed1Color(bool r, bool g, bool b);
void setLed2Color(bool r, bool g, bool b);
void setBothLedsColor(bool r, bool g, bool b);
void setLed1FromTable(int colorIndex);
void setLed2FromTable(int colorIndex);
void setBothLedsFromTable(int colorIndex);
void allLedsOff();
void ledOff();
void updateRgbAnimation();

// ============================================================
// 595 移位寄存器操作
// ============================================================

// 向595发送一个字节并锁存输出
void shiftOut595(byte data) {
  digitalWrite(NLSF_LATCH_PIN, LOW);
  shiftOut(NLSF_DATA_PIN, NLSF_CLOCK_PIN, MSBFIRST, data);
  digitalWrite(NLSF_LATCH_PIN, HIGH);
}

// 设置595某一位的值，并立即更新输出
void setShiftBit(int bit, bool value) {
  if (value) {
    shiftRegState |= (1 << bit);
  } else {
    shiftRegState &= ~(1 << bit);
  }
  shiftOut595(shiftRegState);
}

// 清空595所有输出
void clearShiftReg() {
  if (RGB_INVERT) {
    shiftRegState = 0xFF;      // 共阳极： 全部HIGH = 全灭
  } else {
    shiftRegState = 0x00;
  }

  shiftOut595(shiftRegState);
}


// ============================================================
// RGB LED 颜色控制
// ============================================================

// 设置 LED1 的 RGB 的颜色 r/g/b: true = 亮，false = 灭
void setLed1Color(bool r, bool g, bool b) {
  if (RGB_INVERT) {
    r = !r;
    g = !g;
    b = !b;
  }

  if (r) shiftRegState |= (1 << LED1_RED_BIT);
  else shiftRegState &= ~(1 << LED1_RED_BIT);

  if (g) shiftRegState |= (1 << LED1_GREEN_BIT);
  else shiftRegState &= ~(1 << LED1_GREEN_BIT);

  if (b) shiftRegState |= (1 << LED1_BLUE_BIT);
  else shiftRegState &= ~(1 << LED1_BLUE_BIT);

  shiftOut595(shiftRegState);
}

// 设置 LED2 的 RGB 的颜色 r/g/b: true = 亮，false = 灭
void setLed2Color(bool r, bool g, bool b) {
  if (RGB_INVERT) {
    r = !r;
    g = !g;
    b = !b;
  }

  if (r) shiftRegState |= (1 << LED2_RED_BIT);
  else shiftRegState &= ~(1 << LED2_RED_BIT);

  if (g) shiftRegState |= (1 << LED2_GREEN_BIT);
  else shiftRegState &= ~(1 << LED2_GREEN_BIT);

  if (b) shiftRegState |= (1 << LED2_BLUE_BIT);
  else shiftRegState &= ~(1 << LED2_BLUE_BIT);

  shiftOut595(shiftRegState);
}


// 同时设置两个LED相同的颜色
void setBothLedsColor(bool r, bool g, bool b) {
  setLed1Color(r, g, b);
  setLed2Color(r, g, b);
}


// 用颜色表索引设置LED1
void setLed1FromTable(int colorIndex) {
  int idx = colorIndex % COLOR_COUNT;
  setLed1Color(COLOR_TABLE[idx][0], COLOR_TABLE[idx][1], COLOR_TABLE[idx][2]);
}


// 用颜色表索引设置LED2
void setLed2FromTable(int colorIndex) {
  int idx = colorIndex % COLOR_COUNT;
  setLed2Color(COLOR_TABLE[idx][0], COLOR_TABLE[idx][1], COLOR_TABLE[idx][2]);
}


// 用颜色表索引同时设置两个LED
void setBothLedsFromTable(int colorIndex) {
  setLed1FromTable(colorIndex);
  setLed2FromTable(colorIndex);
}


// 关闭所有LED
void allLedsOff() {
  if (RGB_INVERT) {
    shiftRegState = 0xFF;   // 共阳极全灭
  } else {
    shiftRegState = 0x00;   // 共阴极全灭
  }
  shiftOut595(shiftRegState);
}


// LED 相关更新
void updateLedAction() {
  unsigned long now = millis();
  switch (currentLedMode) {
    case MODE_OFF:            // 关灯
    case MODE_ON:             // 常亮
      break;
    case MODE_BLINK:          // 闪烁
      if (now - lastBlinkTime >= BLINK_INTERVAL) {
        // 重置最后记录的时间
        lastBlinkTime = now;

        // 闪烁状态取反
        blinkState = !blinkState;
        // 根据闪烁状态设置LED亮灭
        if (blinkState)
        {
          ledcWrite(LED4_PIN, 255);      // 亮
        }
        else
        {
          ledcWrite(LED4_PIN, 0);         // 灭
        }
      }
      break;
    case MODE_BREATHE:        // 呼吸灯
      if (now - lastBreatheTime >= BREATHE_INTERVAL) {
        // 重置最后记录的时间
        lastBreatheTime = now;
        breathBrighness += breathDirection;
        if (breathBrighness >= 255) {
          breathBrighness = 255;
          breathDirection = -5;
        }
        if (breathBrighness <= 0) {
          breathBrighness = 0;
          breathDirection = 5;
        }
        ledcWrite(LED4_PIN, breathBrighness); 
      }
      break;
  }
}
// RGB LED 动画更新 (在 loop 中持续调用)
void updateRgbAnimation() {
  unsigned long now = millis();
  switch (currentRgbMode) {
    case RGB_IDLE:
    case RGB_RED_STEADY:
    case RGB_BLUE_STEADY:
    case RGB_GREEN_STEADY:
      // 常亮模式不需要持续更新
      break;
    case RGB_COLOR_CHANGE:
      // 每1秒切换一次颜色
      if (now - rgbLastUpdate >= 1000) {
        rgbLastUpdate = now;
        setBothLedsFromTable(rgbColorIndex);
        // 更新OLED 显示当前颜色名称
        showRgbStatus("Color Change", rgbColorIndex);
        rgbColorIndex = (rgbColorIndex + 1) % COLOR_COUNT;
      }
      break;
    case RGB_RAINBOW:
      // 每 500ms 切换，两个 LED 显示不同颜色
      if (now - rgbLastUpdate >= 500) {
        rgbLastUpdate = now;
        setLed1FromTable(rgbColorIndex);
        setLed2FromTable(rgbColorIndex + 3); // LED2偏移3个颜色
        showRgbStatus("Rainbow", rgbColorIndex);
        rgbColorIndex  = (rgbColorIndex + 1) % COLOR_COUNT;
      }
      break;
    case RGB_BREATHING:
      // 呼吸灯效果：用快速开关模拟亮度变化
      if (now - rgbBreathLastUpdate >= 10) {
        rgbBreathLastUpdate = now;

        // rgbBreathStep:0~100 模拟亮度
        // 在每个周期内，LED亮 rgbBreathStep% 的时间
        int phase = (now / 10) % 100;
        bool ledOn = (phase < rgbBreathStep);
        setBothLedsColor(ledOn, ledOn, ledOn); // 白色呼吸

        // 更新亮度步进
        static unsigned long lastStepTime = 0;
        if (now - lastStepTime >= 30) {
          lastStepTime = now;
          if (rgbBreathDir) {
            rgbBreathStep += 2;
            if (rgbBreathStep >= 100) {
              rgbBreathStep = 100;
              rgbBreathDir = false;
            }
          } else {
            rgbBreathStep -= 2;
            if (rgbBreathStep <= 0) {
              rgbBreathStep = 0;
              rgbBreathDir = true;
            }
          }
        }
      }
      break;
    case RGB_COLOR_WHEEL:
      if (now - rgbLastUpdate >= 300) {
        rgbLastUpdate = now;
        setLed1FromTable(rgbColorIndex);
        setLed2FromTable(rgbColorIndex + 1);
        showRgbStatus("Color Wheel", rgbColorIndex);
        rgbColorIndex = (rgbColorIndex + 1) % COLOR_COUNT;
      }
      break;
  }
}

void showRgbStatus(const char* modeName, int colorIdx) {
  // 只更新底部状态区域，不清除整个屏幕
  display.fillRect(0, 50, 128, 14, SSD1306_BLACK);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(4, 54);

  const char* colorNames[] = {"RED", "GREEN", "BLUE", "YELLOW", "CYAN", "PURPLE", "WHITE"};
  int idx = colorIdx % COLOR_COUNT;
  display.print(modeName);
  display.print(": ");
  display.print(colorNames[idx]);
  display.display();
}


void testEachBit() {
  // 逐位测试，每次只亮一个位，观察哪个灯亮什么颜色
  for (int bit = 0; bit < 8; bit++) {
    // 共阳极：0=亮，所以全部设为1(灭)，只把测试位设为0(亮)
    byte data = 0xFF;         // 全灭
    data &= ~(1 << bit);     // 只亮这一位

    Serial.print("Testing bit ");
    Serial.print(bit);
    Serial.print(" (LSBFIRST → Q");
    char qName = 'A' + bit;
    Serial.print(qName);
    Serial.print(") → data=0x");
    Serial.println(data, HEX);

    digitalWrite(NLSF_LATCH_PIN, LOW);
    shiftOut(NLSF_DATA_PIN, NLSF_CLOCK_PIN, LSBFIRST, data);
    digitalWrite(NLSF_LATCH_PIN, HIGH);

    delay(5000);  // 每个位亮2秒，观察是哪个灯的哪个颜色
  }

  // 测试完全灭
  digitalWrite(NLSF_LATCH_PIN, LOW);
  shiftOut(NLSF_DATA_PIN, NLSF_CLOCK_PIN, LSBFIRST, 0xFF);
  digitalWrite(NLSF_LATCH_PIN, HIGH);
}



void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("ESP32 Joystick Menu System Starting...");

  // 设置按键引脚（输入+内部上拉）
  pinMode(BTN_PIN, INPUT_PULLUP);

  // 摇杆引脚初始化
  pinMode(VERT_PIN, INPUT);
  pinMode(HORZ_PIN, INPUT);
  pinMode(SEL_PIN, INPUT_PULLUP);


  // NLSF 595 引脚定义
  pinMode(NLSF_DATA_PIN, OUTPUT);
  pinMode(NLSF_LATCH_PIN, OUTPUT);
  pinMode(NLSF_CLOCK_PIN, OUTPUT);


  // LED4 引脚定义
  ledcAttach(LED4_PIN, PWM_FREQ, PWM_RESOLUTION);
  ledcWrite(LED4_PIN, 0);

  // 初始化595，全部输出LOW
  clearShiftReg();

  // 初始化 I2C，指定 SDA 和 SCL 引脚
  Wire.begin(I2C_SDA, I2C_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
  {
    Serial.println("SSD1306 初始化失败！");
    while (true)
    {
      delay(1000);
    }
  }
  Serial.println("OLED 初始化成功！");

  // 初始化主菜单
  menuStack[0].items = mainMenu;
  menuStack[0].count = mainMenuCount;
  menuStack[0].selectedIndex = 0;
  menuStack[0].scrollOffset = 0;
  menuDepth = 0;

  // 临时测试
  // testEachBit();

  drawMenu();
}

void loop() {
  JoyDirection dir = readJoystick();

  if (inActionMode) {
    if (dir == JOY_LEFT || dir == JOY_PRESS) {
      inActionMode = false;
      actionLabel = NULL;

      // 如果有滚动，先停止
      if (isScroll) {
        display.stopscroll();
        isScroll = false;
      }
      // 停止RGB动画，关灯
      currentRgbMode = RGB_IDLE;
      // 停止 LED 动画，关灯
      currentLedMode = MODE_OFF;

      allLedsOff();
      ledOff();

      // goBack();
      Serial.println("press back (from action)");  // ← 添加打印
      drawMenu();
    }
    // 持续更新RGB动画
    updateRgbAnimation();
    // 持续更新LED动画
    updateLedAction();
    return;
  }

  if (dir != JOY_NONE) {
    handleJoystickInput(dir);
  }

  updateTextScroll();
  // 即使在菜单界面，也更新RGB动画（如果有正在运行的）
  updateRgbAnimation();
}

/*
  读取摇杆方向（需要带消抖和长安连续触发）
  位置表：
    Top-Left(1023,1023)  Top(512,1023)  Top-Right(0,1023)
    Left(1023,512)       Center(512,512) Right(0,512)
    Bottom-Left(1023,0)  Bottom(512,0)  Bottom-Right(0,0)
  换句话来说：
    VERT 大 = 上     小 = 下
    HORZ 大 = 左     小 = 右
    （1023,1023)        (0,1023)
              (512,512)
     (1023,0)           (0,0)
*/
// 获取摇杆方向
JoyDirection readJoystick() {
  int hVal = analogRead(HORZ_PIN);
  int vVal = analogRead(VERT_PIN);
  // Serial.print("水平值：");
  // Serial.print(hVal);    // 打印第一个值
  // Serial.print(" | 垂直值：");
  // Serial.print(vVal);  // 打印第二个值并换行
  JoyDirection dir = JOY_NONE;
  if (vVal > JOY_CENTER + JOY_THRESHOLD) {
    dir = JOY_UP;
  } else if (vVal < JOY_CENTER - JOY_THRESHOLD ) {
    dir = JOY_DOWN;
  } else if (hVal > JOY_CENTER + JOY_THRESHOLD) {
    dir = JOY_LEFT;
  } else if (hVal < JOY_CENTER - JOY_THRESHOLD) {
    dir = JOY_RIGHT;
  }
  // Serial.print(" | 方向：");
  // Serial.println(dir);

  // 按钮检测（消抖）

  // 读取当前按钮的物理状态 （HIGH=未按下/LOW=按下）
  bool reading = digitalRead(SEL_PIN);

  // 如果当前读取值与上一次不一致，则说明按钮发生了状态
  if (reading != lastButtonState_sel) {
    lastDebounceTime_sel = millis();  // 重置消抖计时，重新开始计时
  }
  // 判断 距离上次状态变化 是否 已经超过消抖延时
  if ((millis() - lastDebounceTime_sel) > DEBOUNCE_DELAY_SEL) {
    // 稳定后的状态 与 当前记录的按钮状态不同 - 更新状态
    if (reading != currentButtonState_sel) {
      currentButtonState_sel  = reading;
      // 如果稳定后的状态是LOW，代表按钮真正被按下
      if (currentButtonState_sel == LOW) {
        // 记录最新状态
        lastButtonState_sel = reading;
        return JOY_PRESS;
      }
    }
  }
  // 更新上一次按钮状态，为下一次循环做准备
  lastButtonState_sel = reading;

  // 方向键消抖： 首次触发 + 长按连续
  unsigned long now = millis();

  if (dir == JOY_NONE) {
    lastJoyDir = JOY_NONE;
    joyFirstTriggered = false;
    return JOY_NONE;
  }

  if (dir != lastJoyDir) {
    lastJoyDir = dir;
    joyHoldStart = now;
    joyFirstTriggered = true;
    lastJoyRepeat = now;
    return dir;
  }

  // 同方向持续按住
  if (joyFirstTriggered) {
    if (now - lastJoyRepeat > JOY_REPEAT_FIRST) {
      joyFirstTriggered = false;
      lastJoyRepeat = now;
      return dir;
    }
  } else {
    if (now - lastJoyRepeat > JOY_REPEAT_NEXT) {
      lastJoyRepeat = now;
      return dir;
    }
  }

  return JOY_NONE;

}

// 处理摇杆输入
void handleJoystickInput(JoyDirection dir) {
  MenuLevel &cur = menuStack[menuDepth];

  switch (dir) {
    case JOY_UP:
      if (cur.selectedIndex > 0) {
        cur.selectedIndex--;
        if (cur.selectedIndex < cur.scrollOffset) {
          cur.scrollOffset = cur.selectedIndex;
        }
        resetTextScroll();
        drawMenu();
      }
      break;

    case JOY_DOWN:
      if (cur.selectedIndex < cur.count - 1) {
        cur.selectedIndex++;
        if (cur.selectedIndex >= cur.scrollOffset + VISIBLE_LINES) {
          cur.scrollOffset = cur.selectedIndex - VISIBLE_LINES + 1;
        }
        resetTextScroll();
        drawMenu();
      }
      break;
    case JOY_RIGHT:
    case JOY_PRESS:
      enterSelected();
      break;

    case JOY_LEFT:
      goBack();
      break;

    default:
      break;
  }
}


// 进入选中的菜单
void enterSelected() {
  MenuLevel &cur = menuStack[menuDepth];
  MenuItem &item = cur.items[cur.selectedIndex];

  if (item.subMenu != NULL && item.subMenuCount > 0) {
    //有子菜单，进入下一层
    if (menuDepth < MAX_MENU_DEPTH - 1) {
      menuDepth++;
      menuStack[menuDepth].items = item.subMenu;
      menuStack[menuDepth].count = item.subMenuCount;
      menuStack[menuDepth].selectedIndex = 0;
      menuStack[menuDepth].scrollOffset = 0;
      resetTextScroll();
      drawMenu();
      Serial.print("Enter submenu:");
      Serial.println(item.label);
    }
  } else if (item.action != NULL) {
    // 叶子节点，执行回调
    Serial.print("Execute: ");
    Serial.println(item.label);

    // 显示执行提示界面
    showActionScreen(item.label);
    delay(500);
    item.action(item.label);
  }
}


// 返回上层菜单
void goBack() {
  Serial.println("press back");

  if (isScroll) {
    display.stopscroll();
    Serial.println("停止滚动");
    isScroll = false;
    display.clearDisplay();
    display.display();
  }
  if (menuDepth > 0) {
    menuDepth--;
    resetTextScroll();
    drawMenu();
    Serial.println("Back to parent menu");
  } else {
    drawMenu();
  }

}


// 显示动作执行后的提示界面
void showActionScreen(const char* label) {
  inActionMode = true;
  actionLabel = label;


  display.clearDisplay();

  // 标题
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("-- Executing --");
  display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  // 显示执行的菜单名
  display.setCursor(4, 20);
  display.print("> ");
  display.println(label);

  // 提示返回
  display.setCursor(4, 50);
  display.print("<< LEFT to back");

  display.display();
}


// 绘制菜单界面
void drawMenu() {
  MenuLevel &cur = menuStack[menuDepth];

  display.clearDisplay();

  // 绘制标题栏
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  if (menuDepth == 0) {
    display.setCursor(2, 3);
    display.print("[Main Menu]");
  } else {
    // 显示父菜单的选中项名称作为标题
    MenuLevel &parent = menuStack[menuDepth - 1];
    MenuItem &parentItem = parent.items[parent.selectedIndex];
    display.setCursor(2, 3);
    display.print("< ");
    // --- 核心优化部分 ---
    String labelText = parentItem.label;
    
    // 假设屏幕一行最多显示 14 个字符
    // 因为前面已经打印了 "< "（占 2 个字符），所以留给文字的最多只有 12 个字符
    const int MAX_LEN = 18; 

    if (labelText.length() > MAX_LEN) {
        // 截取前 9 个字符，并在末尾拼接 "..."
        // 9 字符 + 3 字符(...) = 12 字符，完美适配
        labelText = labelText.substring(0, MAX_LEN - 3) + "...";
    }
    
    display.print(labelText);
    // display.print(parentItem.label);
  }
  // 标题分割线
  display.drawLine(0, TITLE_HEIGHT - 2, SCREEN_WIDTH - 1, TITLE_HEIGHT - 2, SSD1306_WHITE);

  // 绘制菜单项
  int startY = TITLE_HEIGHT;

  for (int i = 0; i < VISIBLE_LINES && (cur.scrollOffset + i) < cur.count; i++) {
    int itemIndex = cur.scrollOffset + i;
    int y = startY + i * LINE_HEIGHT;
    bool isSelected = (itemIndex == cur.selectedIndex);

    if (isSelected) {
      // 选中项背景（反白效果）
      display.fillRect(0, y, SCREEN_WIDTH, LINE_HEIGHT, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);

      // 三角形箭头
      int arrowX = 1;
      int arrowY = y + 2;
      display.fillTriangle(
        arrowX, arrowY,
        arrowX, arrowY + 7,
        arrowX + 5, arrowY + 3,
        SSD1306_BLACK
      );

      // 文字 （带水平滚动）
      drawScrollingText(cur.items[itemIndex].label, TEXT_AREA_X, y + 2, true);

      // 如果有子菜单，显示右箭头
      if (cur.items[itemIndex].subMenu != NULL) {
        // 先画一小块反白背景确保箭头可见
        display.fillRect(SCREEN_WIDTH - 10, y, 10, LINE_HEIGHT, SSD1306_WHITE);
        display.setCursor(SCREEN_WIDTH - 8, y + 2);
        display.setTextColor(SSD1306_BLACK);
        display.print(">");
      }
    } else {
      display.setTextColor(SSD1306_WHITE);
      // 文字（静态截断）
      drawStaticText(cur.items[itemIndex].label, TEXT_AREA_X, y + 2);

      // 如果有子菜单，显示右箭头
      if (cur.items[itemIndex].subMenu != NULL) {
        display.setCursor(SCREEN_WIDTH - 8, y + 2);
        display.print(">");
      }
    }
  }
  // 滚动条
  if (cur.count > VISIBLE_LINES) {
    drawScrollBar(cur.scrollOffset, cur.count, VISIBLE_LINES);
  }
  display.display();
}

// 绘制静态文字（超出截断）
void drawStaticText(const char* text, int x, int y) {
  int maxChars = (SCREEN_WIDTH - x - 10) / FONT_WIDTH;
  char buf[32];
  int len = strlen(text);
  if (len > maxChars) {
    strncpy(buf, text, maxChars - 2);
    buf[maxChars - 2] = '.';
    buf[maxChars - 1] = '.';
    buf[maxChars] = '\0';
  } else {
    strncpy(buf, text, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
  }

  display.setCursor(x, y);
  display.print(buf);

}

// 绘制可滚动文字（选中项，超宽时水平滚动）
void drawScrollingText(const char* text, int x, int y, bool isSelected) {
  int textPixelWidth = strlen(text) * FONT_WIDTH;
  int availableWidth = SCREEN_WIDTH - x - 12;

  if (textPixelWidth <= availableWidth) {
    // 没超出，直接显示
    display.setCursor(x, y);
    display.print(text);
  } else {
    //超出，需要滚动
    int maxScroll = textPixelWidth - availableWidth;
    int offset = textScrollOffset;
    if (offset > maxScroll) offset = maxScroll;

    // 逐字符绘制，只画在可见区域内的
    int len = strlen(text);
    for (int i = 0; i < len; i++) {
      int charX = x + i * FONT_WIDTH - offset;
      if (charX >= x && charX + FONT_WIDTH <= x + availableWidth) {
        display.setCursor(charX, y);
        char c[2] = {text[i], '\0'};
        display.print(c);
      }
    }
  }
}


// 更新文字水平滚动
void updateTextScroll() {
  MenuLevel &cur = menuStack[menuDepth];
  MenuItem &item = cur.items[cur.selectedIndex];

  int textPixelWidth = strlen(item.label) * FONT_WIDTH;
  int availableWidth = SCREEN_WIDTH - TEXT_AREA_X - 12;

  // 文字没超出，不需要滚动
  if (textPixelWidth <= availableWidth) {
    return;
  }

  unsigned long now = millis();
  if (now - lastTextScrollTime < TEXT_SCROLL_INTERVAL) {
    return;
  }
  lastTextScrollTime = now;

  int maxScroll = textPixelWidth - availableWidth;

  // 暂停处理
  if (textScrollPause > 0) {
    textScrollPause--;
    if (textScrollPause == 0) {
      if (textScrollOffset >= maxScroll) {
        textScrollOffset = 0;
        textScrollPause = TEXT_SCROLL_PAUSE_COUNT;
        drawMenu();
        return;
      }
    }
    return;
  }
  textScrollOffset += 2;

  if (textScrollOffset >= maxScroll) {
    textScrollOffset = maxScroll;
    textScrollPause = TEXT_SCROLL_PAUSE_COUNT;
  }

  drawMenu();

}

// 重置文字滚动状态
void resetTextScroll() {
  textScrollOffset = 0;
  textScrollPause = TEXT_SCROLL_PAUSE_COUNT;
  lastTextScrollTime = millis();
}

// 绘制右侧滚动条
void drawScrollBar(int offset, int totalItems, int visibleItems) {
  int barX = SCREEN_WIDTH - 3;
  int barAreaY = TITLE_HEIGHT;
  int barAreaH = SCREEN_HEIGHT - TITLE_HEIGHT;

  // 滚动条轨道
  display.drawLine(barX + 1, barAreaY, barX + 1, SCREEN_HEIGHT - 1, SSD1306_WHITE);

  // 滚动条滑块
  int sliderH = max(4, barAreaH * visibleItems / totalItems);
  int sliderY = barAreaY;

  if (totalItems > visibleItems) {
    sliderY = barAreaY + (barAreaH - sliderH) * offset / (totalItems - visibleItems);
  }
  display.fillRect(barX, sliderY, 3, sliderH, SSD1306_WHITE);
}

void onTextDemo(const char* label) {
  Serial.print("[Action] ");
  Serial.println(label);

  // TODO: 在这里实现 具体 逻辑
  display.clearDisplay();

  // 小号字体
  display.setTextSize(1); // 1 倍大小：6x8 像素/字符
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Size 1:Hello!");

  // 中号字体
  display.setTextSize(2); // 2 倍大小：12x16 像素/字符
  display.setCursor(0, 16);
  display.println("Size 2");

  // 大号字体
  display.setTextSize(3); // 3 倍大小：18x24 像素/字符
  display.setCursor(0, 40);
  display.println("BIG");

  display.display();

  Serial.println("显示：文字演示");

}

void onShapesDemo(const char* label) {
  Serial.print("[Action] ");
  Serial.println(label);

  // TODO: 在这里实现 具体 逻辑
  display.clearDisplay();

  // 绘制标题
  display.setTextSize(1); // 1 倍大小：6x8 像素/字符
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Shapes Demo");

  // 画空心矩形 drawRect(x,y,w,h,colore)
  display.drawRect(5, 15, 30, 20, SSD1306_WHITE);
  // 画实心矩形
  display.fillRect(45, 15, 30, 20, SSD1306_WHITE);
  // 画空心圆 drawCircle(圆心x,圆心y,半径,颜色)
  display.drawCircle(100, 25, 12, SSD1306_WHITE);
  // 画实心圆
  display.fillCircle(20, 52, 8, SSD1306_WHITE);
  // 画三角形 drawTriangle(x1,y1,x2,y2,x3,y3,颜色)
  display.drawTriangle(50, 55, 70, 40, 90, 55, SSD1306_WHITE);
  // 画虚线效果（用多个点）
  for (int i = 0; i < 128; i += 3)
  {
    display.drawPixel(i, 12, SSD1306_WHITE);
  }

  display.display();
}


void onProgressBarDemo(const char* label) {
  Serial.print("[Action] ");
  Serial.println(label);

  // TODO: 在这里实现 具体 逻辑
  counter = 0;
  display.clearDisplay();

  // 绘制标题
  display.setTextSize(1); // 1 倍大小：6x8 像素/字符
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Progress Bar:");

  // 画进度条外框
  display.drawRect(10, 25, 108, 15, SSD1306_WHITE);
  display.display();
  Serial.println("显示：进度条演示");

  updateProgressBar();
}


void updateProgressBar() {
  int counter = 0;
  while (true) {
    if (counter > 100)
      return;

    // 重绘进度条区域
    // 先用黑色填充内部区域，清楚旧的进度
    display.fillRect(11, 26, 106, 13, SSD1306_BLACK);
    // 计算进度条填充宽度
    int barWidth = map(counter, 0, 100, 1, 104); // map(值, 原始最小, 原始最大, 目标最小, 目标最大)

    display.fillRect(12, 27, barWidth, 11, SSD1306_WHITE);

    // 显示百分比数字
    display.fillRect(40, 45, 50, 15, SSD1306_BLACK); // 清楚旧数字
    display.setTextSize(2);
    display.setCursor(40, 45);
    display.print(counter);
    display.print("%");

    display.display();
    counter += 2; // 每次增加2%
    delay(50);
  }
}


void onScrollDemo(const char* label) {
  Serial.print("[Action] ");
  Serial.println(label);

  // TODO: 在这里实现 具体 逻辑
  display.clearDisplay();

  // 绘制标题
  display.setTextSize(2); // 1 倍大小：6x8 像素/字符
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Scrolling");
  display.println("Demo!!");

  display.setTextSize(1);
  display.setCursor(0, 40);
  display.println("This text will");
  display.println("scroll around... ");
  display.display();
  // 启动水平向左滚动
  // startscrollleft(起始页，结束页)
  //     页(page)是SSD1306的显示单位，每页8个像素高
  //     128×64屏幕有8页（0~7）
  //     0x00 = 第0页（最上面8行像素）；0x07 = 第7页（最下面8行像素）
  // 其他滚动函数：
  //     startscrollright(起始页, 结束页)     → 向右滚
  //     startscrolldiagleft(起始页, 结束页)  → 左斜滚
  //     startscrolldiagright(起始页, 结束页) → 右斜滚
  //     stopscroll()                         → 停止滚动
  isScroll = true;
  display.startscrollleft(0x00, 0x07);


  Serial.println("显示：滚动演示");
}


void ledOff() {
  ledcWrite(LED4_PIN, 0);
}

void onLedAction(const char* label) {
  Serial.print("[LED Action] ");
  Serial.println(label);

  // 先停止之前的动画，关灯
  currentLedMode = MODE_OFF;
  ledOff();

  // 重置动画参数
  lastBlinkTime = millis();

  if (strcmp(label, "Steady On") == 0) {
    currentLedMode = MODE_ON;
    ledcWrite(LED4_PIN, 255);       // LED4 常亮
  } else if (strcmp(label, "Blinking") == 0) {
    currentLedMode = MODE_BLINK;
    lastBlinkTime = 0;


  } else if (strcmp(label, "Breathing") == 0) {
    currentLedMode = MODE_BREATHE;
    lastBreatheTime = 0;

  } else {
    currentLedMode = MODE_OFF;
    ledOff();
  }
}



void onRgbLedDemo(const char* label) {
  Serial.print("[RGB LED Action] ");
  Serial.println(label);

  // TODO: 在这里实现 具体 逻辑
  // 先停止之前的动画，关灯
  currentRgbMode = RGB_IDLE;
  allLedsOff();

  // 重置动画参数
  rgbColorIndex = 0;
  rgbBreathStep = 0;
  rgbBreathDir = true;
  rgbLastUpdate = millis();
  rgbBreathLastUpdate = millis();
  if (strcmp(label, "Red steady on") == 0) {
    // 红色常亮
    currentRgbMode = RGB_RED_STEADY;
    setBothLedsColor(true, false, false);

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("RGB LED Demo");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    // 画一个红色指示圆
    display.setTextSize(2);
    display.setCursor(10, 25);
    display.print("RED");

    display.fillCircle(100, 32, 12, SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(93, 29);
    display.setTextColor(SSD1306_BLACK);
    display.print("ON");

    display.setTextColor(SSD1306_WHITE);
    display.setCursor(4, 54);
    display.print("<< LEFT to back");
    display.display();
  } else if (strcmp(label, "Blue steady on") == 0) {
    // ---- 蓝色常亮 ----
    currentRgbMode = RGB_BLUE_STEADY;
    setBothLedsColor(false, false, true);

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("RGB LED Demo");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    display.setTextSize(2);
    display.setCursor(10, 25);
    display.print("BLUE");

    display.fillCircle(100, 32, 12, SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(93, 29);
    display.setTextColor(SSD1306_BLACK);
    display.print("ON");

    display.setTextColor(SSD1306_WHITE);
    display.setCursor(4, 54);
    display.print("<< LEFT to back");
    display.display();

  } else if (strcmp(label, "Green steady on") == 0) {
    // ---- 绿色常亮 ----
    currentRgbMode = RGB_GREEN_STEADY;
    setBothLedsColor(false, true, false);

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("RGB LED Demo");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    display.setTextSize(2);
    display.setCursor(10, 25);
    display.print("GREEN");

    display.fillCircle(100, 32, 12, SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(93, 29);
    display.setTextColor(SSD1306_BLACK);
    display.print("ON");

    display.setTextColor(SSD1306_WHITE);
    display.setCursor(4, 54);
    display.print("<< LEFT to back");
    display.display();
  } else if (strcmp(label, "Change color every 1s") == 0) {
    // ---- 每秒变色 ----
    currentRgbMode = RGB_COLOR_CHANGE;
    rgbLastUpdate = 0;  // 立即触发第一次

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("RGB LED Demo");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    display.setTextSize(1);
    display.setCursor(4, 16);
    display.print("Mode: Color Change");
    display.setCursor(4, 28);
    display.print("Interval: 1 second");

    display.setCursor(4, 42);
    display.print("Colors: R G B Y C P W");

    display.setCursor(4, 54);
    display.print("<< LEFT to back");
    display.display();
  } else if (strcmp(label, "Rainbow cycle") == 0) {
    // ---- 彩虹循环 ----
    currentRgbMode = RGB_RAINBOW;
    rgbLastUpdate = 0;

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("RGB LED Demo");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    display.setTextSize(1);
    display.setCursor(4, 16);
    display.print("Mode: Rainbow Cycle");
    display.setCursor(4, 28);
    display.print("LED1 & LED2 show");
    display.setCursor(4, 38);
    display.print("different colors");

    display.setCursor(4, 54);
    display.print("<< LEFT to back");
    display.display();

  } else if (strcmp(label, "Breathing effect") == 0) {
    // ---- 呼吸灯 ----
    currentRgbMode = RGB_BREATHING;

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("RGB LED Demo");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    display.setTextSize(1);
    display.setCursor(4, 16);
    display.print("Mode: Breathing");
    display.setCursor(4, 28);
    display.print("White LED fades");
    display.setCursor(4, 38);
    display.print("in and out...");

    display.setCursor(4, 54);
    display.print("<< LEFT to back");
    display.display();

  } else if (strcmp(label, "Color wheel") == 0) {
    // ---- 色轮 ----
    currentRgbMode = RGB_COLOR_WHEEL;
    rgbLastUpdate = 0;

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("RGB LED Demo");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    display.setTextSize(1);
    display.setCursor(4, 16);
    display.print("Mode: Color Wheel");
    display.setCursor(4, 28);
    display.print("Adjacent colors");
    display.setCursor(4, 38);
    display.print("rotating...");

    display.setCursor(4, 54);
    display.print("<< LEFT to back");
    display.display();

  } else if (strcmp(label, "All OFF") == 0) {
    // ---- 全部关闭 ----
    currentRgbMode = RGB_IDLE;
    allLedsOff();

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("RGB LED Demo");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    display.setTextSize(2);
    display.setCursor(30, 25);
    display.print("OFF");

    display.setTextSize(1);
    display.setCursor(4, 54);
    display.print("<< LEFT to back");
    display.display();
  }
}


void onButtonDemo(const char* label) {
  Serial.print("[LED Action] ");
  Serial.println(label);

  // TODO: 在这里实现 具体 逻辑
}