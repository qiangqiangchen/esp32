// 引脚定义
const int LED_PIN = 4;
const int BUTTON_PIN = 5;

// PWM 参数
const int PWM_FREQ = 5000;
const int PWM_RESOLUTION = 8;

// 模式定义
enum LightMode
{
    MODE_OFF = 0,    // 关灯
    MODE_ON = 1,     // 常亮
    MODE_BLINK = 2,  // 闪烁
    MODE_BREATHE = 3 // 呼吸灯
};

// 全局变量
LightMode currentMode = MODE_OFF; // 当前灯光模式   ，开机默认“关灯”

// 按键消抖相关变量
bool lastButtonState = HIGH;             // 上一次读到的按键状态（HIGH=未按下）
bool currentButtonState = HIGH;          // 当前确认的按键状态
unsigned long lastDebounceTime = 0;      // 上一次按键状态变化的时间
const unsigned long DEBOUNCE_DELAY = 50; // 消抖延时：50毫秒

// 闪烁模式相关变量
unsigned long lastBlinkTime = 0;          // 上一次 LED 切换亮灭的时间
const unsigned long BLINK_INTERVAL = 300; // 闪烁间隔：300毫秒
bool blinkState = false;                  // 当前闪烁状态

// 呼吸灯模式相关变量
int breathBrighness = 0;                   // 当前亮度值（0~255）
int breathDirection = 5;                   // 亮度变化步长和方向
unsigned long lastBreatheTime = 0;         // 上一次更新亮度的时间
const unsigned long BREATHE_INTERVAL = 15; // 亮度更新间隔：15毫秒

void setup()
{
    // 1. 初始化串口
    Serial.begin(115200);
    Serial.println("系统启动");

    // 设置按键引脚（输入+内部上拉）
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    // 设置LED引脚
    // 它会分配一个空闲的PWM通道，设置频率和分辨率，并且把通道绑定到指定引脚
    ledcAttach(LED_PIN, PWM_FREQ, PWM_RESOLUTION);
    // 确保LED初始状态为关闭
    ledcWrite(LED_PIN, 0);
    Serial.println("初始化完成,当前模式：关灯");
}

void loop()
{
    // 第一步：检测按键
    checkButton();
    // 第二部：根据模式控制LED
    switch (currentMode)
    {
    case MODE_OFF: // 关灯
        ledcWrite(LED_PIN, 0);
        break;
    case MODE_ON: // 常量
        ledcWrite(LED_PIN, 255);
        break;

    case MODE_BLINK: // 闪烁模式
        handleBlink();
        break;

    case MODE_BREATHE: // 呼吸灯模式
        handleBreathe();
        break;
    }
}

// ============================================================
//  checkButton() - 按键检测函数（带消抖）
// ============================================================
/*
 * 这个函数的工作流程：
 *
 * 1. 读取按键当前的电平状态
 * 2. 如果状态和上次不同，重置消抖计时器
 * 3. 如果状态持续稳定超过50毫秒，才认为是真正的按键动作
 * 4. 检测"下降沿"（从HIGH变为LOW），即按下的瞬间
 * 5. 切换到下一个灯光模式
 *
 * 为什么不用 delay() 来消抖？
 *   delay() 会让整个程序暂停，导致LED闪烁/呼吸效果卡顿。
 *   用 millis() 计时的方式，程序可以同时处理按键和LED效果。
 */
void checkButton()
{
    // 读取按键一脚地当前状态
    bool reading = digitalRead(BUTTON_PIN);
    // reading = HIGH(1)  -> 按键松开
    // reading = LOW(0)  -> 按键按下

    // 如果读取到的状态和上一次的不同
    if (reading != lastButtonState)
    {
        lastDebounceTime = millis(); // 记录状态变化的时间
    }
    // 判断是否超出了DEBOUNCE_DELAY
    if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY)
    {
        // 如果读取到的状态和上一次不同，说明用户按下了按钮
        if (reading != currentButtonState)
        {
            // 更新确认的状态
            currentButtonState = reading;
            // 检测按下动作（从松开变为按下，即下降沿 HIGH->LOW）
            if (currentButtonState == LOW)
            {
                // 切换下一个模式
                switchToNextMode();
            }
        }
    }
    // 保存本次读取的状态，供下次循环比较
    lastButtonState = reading;
}

void switchToNextMode()
{
    // 循环切换各种模式
    int nextMode = ((int)currentMode + 1) % 4;
    currentMode = (LightMode)nextMode;

    // 每次切换模式时，重置各种状态变量，这样切换到新模式时，效果会从头开始
    blinkState = false;         // 闪烁状态重置
    breathBrighness = 0;       // 呼吸灯亮度从0开始
    breathDirection = 5;        // 呼吸灯方向为“变亮”
    lastBlinkTime = millis();   // 重置闪烁计时
    lastBreatheTime = millis(); // 重置呼吸灯计时
    //串口就打印当前模式
    Serial.print("模式切换->");
    switch (currentMode)
    {
    case MODE_OFF:
        Serial.println("关灯");
        break;
    case MODE_ON:
        Serial.println("常亮");
        break;
    case MODE_BLINK:
        Serial.println("闪烁");
        break;
    case MODE_BREATHE:
        Serial.println("呼吸灯");
        break;
    }
}

void handleBlink()
{
    // 保存当前时间
    unsigned long currentTime = millis();
    // 判断是否到达闪烁间隔
    if (currentTime - lastBlinkTime >= BLINK_INTERVAL)
    {
        // 重置最后记录的时间
        lastBlinkTime = currentTime;
        // 闪烁状态取反
        blinkState = !blinkState;
        // 根据闪烁状态设置LED亮灭
        if (blinkState)
        {
            ledcWrite(LED_PIN, 255);
        }
        else
        {
            ledcWrite(LED_PIN, 0);
        }
    }
}

void handleBreathe()
{   // 记录当前运行时间
    unsigned long currentTime = millis();
    // 判断是否到达闪烁间隔
    if (currentTime - lastBreatheTime >= BREATHE_INTERVAL)
    {   // 重置时间
        lastBreatheTime = currentTime;
        // 更改 PWM 的值 
        breathBrighness += breathDirection;
        if (breathBrighness >= 255)
        {
            breathBrighness = 255;
            breathDirection = -5;
        }
        if (breathBrighness <= 0)
        {
            breathBrighness = 0;
            breathDirection = 5;
        }
        // 将PWM 应用到LED
        ledcWrite(LED_PIN, breathBrighness);
    }
}