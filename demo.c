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
#define NLSF_DATA_PIN 23          // SI
#define NLSF_CLOCK_PIN 5          // RCK
#define NLSF_LATCH_PIN 18         // SCK

// 摇杆定义
#define VERT_PIN 12                // 纵轴输出
#define HORZ_PIN 14                // 水平输出
#define SEL_PIN 27                 // 中间按钮

#define JOY_CENTER 512             // 摇杆中间值 
#define JOY_THRESHOLD 200          // 偏移超过此值才认为有方向输入
#define JOY_REPEAT_FFIRST 300      // 首次触发后的等待时间（ms）
#define JOY_REPEAT_NEXT 150        // 连续触发间隔（ms）

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
  MenuItem* subMene;                      // 子菜单数组指针（NULL=无子菜单）
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
  {"Change color every 1 second", NULL, 0, onRgbLedDemo},
};

const int rgbLedSubMenuCount = sizeof(rgbLedSubMenu) / sizeof(rgbLedSubMenu[0]);

// 主菜单定义
MenuItem mainMenu[] = {
  {"Text Demo", NULL, 0, onTextDemo},
  {"Shapes Demo", NULL, 0, onShapesDemo},
  {"ProgressBar Demo", NULL, 0, onProgressBarDemo},
  {"Scroll Demo", NULL, 0, onScrollDemo },
  {"LED Demo", ledSubMenu, ledSubMenuCount, NULL},
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




void setup()
{
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("ESP32 Joystick Menu System Starting...");

  // 设置按键引脚（输入+内部上拉）
  pinMode(BTN_PIN, INPUT_PULLUP);

  // 摇杆引脚初始化
  pinMode(VERT_PIN, INPUT);
  pinMode(HORZ_PIN, INPUT);
  pinMode(SEL_PIN, INPUT_PULLUP);


  // NLSF 引脚定义
  pinMode(NLSF_DATA_PIN, OUTPUT);
  pinMode(NLSF_LATCH_PIN, OUTPUT);
  pinMode(NLSF_CLOCK_PIN, OUTPUT);


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

  drawMenu();
}

void loop() {
  JoyDirection dir = readJoystick();

  if (inActionMode) {
    // 在动作执行界面，只响应“左”或者“按下”返回
    if (dir == JOY_LEFT || dir == JOY_PRESS) {
      inActionMode = false;
      actionLabel = NULL;
      drawMenu();
    }
    return;
  }

  if (dir != JOY_NONE) {
    handleJoystickInput(dir);
  }

  // 处理选中项的文字水平滚动
  updateTextScroll();

}

// 读取摇杆方向（需要带消抖和长安连续触发）
// 位置表：
//   Top-Left(1023,1023)  Top(512,1023)  Top-Right(0,1023)
//   Left(1023,512)       Center(512,512) Right(0,512)
//   Bottom-Left(1023,0)  Bottom(512,0)  Bottom-Right(0,0)
// 换句话来说：
//   VERT 大 = 上     小 = 下
//   HORZ 大 = 左     小 = 右
//   （1023,1023)        (0,1023)
//             (512,512)
//    (1023,0)           (0,0)


JoyDirection readJoystick() {
  int hVal = analogRead(HORZ_PIN);
  int vVal = analogRead(VERT_PIN);

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
      lastJoyPrpeat = now;
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
      menuStackp[menuDepth].items = item.subMenu;
      menuStackp[menuDepth].count = item.subMenuCount;
      menuStackp[menuDepth].selectedIndex = 0;
      menuStackp[menuDepth].scrollOffset = 0;
      resetTextScroll();
      drawMenu();
      Serial.print("Enter submenu:");
      Serial.println(item.label);
    }
  } else if (item.action ! = NULL) {
    // 叶子节点，执行回调
    Serial.print("Execute: ");
    Serial.println(item.label);

    item.action(item.label);

    // 显示执行提示界面
    showActionScreen(item.label);
  }
}


// 返回上层菜单
void goBack() {
  if (menuDepth > 0) {
    menuDepth--;
    resetTextScroll();
    drawMenu();
    Serial.println("Back to parent menu");
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
  display.setCurosr(4, 20);
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
    display.setCurosr(2, 3);
    display.print("< ");
    display.print(parentItem.label);
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
      if (cur, items[itemIndex].subMenu != NULL) {
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
    buf[maxChars - 2] = ".";
    buf[maxChars - 1] = ".";
    buf[maxChars] = "\0";
  } esle{
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
    display.setCurosr(x, y);
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
void restTextScroll() {
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
    SliderY = barAreaY + (barAreaH - sliderH) * offset / (totalItems - visibleItems);
  }
  display.fillRect(barX, sliderY, 3, sliderH, SSD1306_WHITE);
}

void onTextDemo(const char* label) {
  Serial.print("[Action] ");
  Serial.println(label);

  // TODO: 在这里实现 具体 逻辑
}

void onShapesDemo(const char* label) {
  Serial.print("[Action] ");
  Serial.println(label);

  // TODO: 在这里实现 具体 逻辑
}


void onProgressBarDemo(const char* label) {
  Serial.print("[Action] ");
  Serial.println(label);

  // TODO: 在这里实现 具体 逻辑
}


void onScrollDemo(const char* label) {
  Serial.print("[Action] ");
  Serial.println(label);

  // TODO: 在这里实现 具体 逻辑
}


void onLedAction(const char* label) {
  Serial.print("[LED Action] ");
  Serial.println(label);

  // TODO: 在这里实现 具体 逻辑
}



void onRgbLedDemo(const char* label) {
  Serial.print("[RGB LED Action] ");
  Serial.println(label);

  // TODO: 在这里实现 具体 逻辑
}


void onBUttonDemo(const char* label) {
  Serial.print("[LED Action] ");
  Serial.println(label);

  // TODO: 在这里实现 具体 逻辑
}




// void loop()
// {
//   checkButton();

//   if (lastDemoStep != demoStep) {
//     lastDemoStep = demoStep; // 更新记录
//     switch (demoStep)
//     {
//       //  case 0:
//       //   showWelcome();           // 文字显示演示
//       //   break;
//       case 1:
//         showTextDemo();           // 文字显示演示
//         break;
//       case 2:
//         showShapesDemo();         // 图形绘制演示
//         break;
//       case 3:
//         showProgressBar();         // 进度条演示
//         break;
//       case 4:
//         showScrollDemo();          // 滚动效果演示
//         break;
//       case 5:
//         display.stopscroll();      // 停止滚动
//         demoStep = 0;              // 重置，循环演示
//         showWelcome();             // 显示欢迎画面
//         break;
//     }
//   }

//   // 如果在阶段3（进度条）,持续更新
//   if (demoStep == 3) {
//     updateProgressBar();
//   }
// }

// 检测按钮点击
void checkButton() {
  bool reading = digitalRead(BTN_PIN);
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (reading != currentButtonState) {
      currentButtonState = reading;
      if (currentButtonState == LOW) {
        Serial.println("切换下一个模式");
        demoStep++;

      }
    }
  }
  lastButtonState = reading;
}

void showWelcome()
{
  /*
    OLED 显示的三个基本步骤：
    1. display.clearDisplay()   清空画面缓冲区
    2. 用各种draw/print 函数     在缓冲区中画出内容
    3. display.display()        把缓冲区的内容发送到屏幕上显示

    为什么用缓冲区？
      如果梅花一个东西就发送一次，屏幕会闪烁
      先在内存中画好整个画面，再一次性发送，画面就会流畅
  */

  // 清空缓存区
  display.clearDisplay();

  // 画一个边框
  display.drawRect(0, 0, 128, 64, SSD1306_WHITE);

  // 显示标题文字
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(15, 8);
  display.println("ESP32-C3");

  // 显示副标题
  display.setTextSize(1);
  display.setCursor(20, 35);
  display.println("OLED SSD1306");

  // 显示分割线
  display.drawLine(10, 50, 118, 50, SSD1306_WHITE);

  display.setCursor(30, 54);
  display.println("OLED SSD1306");

  display.display();
  Serial.println("Demo v1.0");
}