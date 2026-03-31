#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // 屏幕宽度（像素）
#define SCREEN_HEIGHT 64 // 屏幕高度（像素）

#define OLED_RESET -1       // 复位引脚（-1 表示与开发板公用复位）
#define SCREEN_ADDRESS 0x3C // I2C 地址

#define I2C_SDA 21 // SDA 引脚
#define I2C_SCL 22 // SCL 引脚

// 创建屏幕对象，之后所有对屏幕的操作都通过display.xx() 来调用
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

int counter = 0;                  // 计数器，用于演示动态数据
unsigned long lastUpdateTime = 0; // 上次更新屏幕的时间
int demoStep = 0;                 // 当前演示阶段

void setup()
{
    // put your setup code here, to run once:
    Serial.begin(115200);
    Serial.println("Hello, ESP32!");

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

    showWelcome();
    delay(3000);
}

void loop()
{   
    // 每3秒切换一个演示阶段
    unsigned long currentTime = millis();
    if (currentTime - lastUpdateTime >= 3000)
    {
        lastUpdateTime = millis();
        demoStep++;
        switch (demoStep)
        {
        case 1:
            showTextDemo();           // 文字显示演示
            break;
        case 2:
            showShapesDemo();         // 图形绘制演示
            break;
        case 3:
            showProgressBar();         // 进度条演示
            break;
        case 4:
            showScrollDemo();          // 滚动效果演示
            break;
        case 5:
            display.stopscroll();      // 停止滚动
            demoStep = 0;              // 重置，循环演示
            showWelcome();             // 显示欢迎画面
            break;
        }
    }
    // 如果在阶段3（进度条）,持续更新
    if(demoStep == 3){
      updateProgressBar();
    }
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

// 文字显示演示
void showTextDemo()
{
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

// 图形绘制演示
void showShapesDemo()
{
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

    Serial.println("显示：图形演示");
}

// 进度条演示
void showProgressBar()
{
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
}

// 更新进度条（在loop中持续调用）
void updateProgressBar()
{

    // 控制更新速度，每隔50毫秒更新一次
    static unsigned long lastBarUpdate = 0;
    unsigned long now = millis();
    if (now - lastBarUpdate < 50)
        return;
    lastBarUpdate = now;
    // 到100%后停止
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
}

// 滚动效果演示
void showScrollDemo()
{
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

    display.startscrollleft(0x00, 0x07);

    Serial.println("显示：滚动演示");
}