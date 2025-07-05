/*
 * ESP32 GitHub Stars Display - LVGL + LovyanGFX Version
 * 基于ESP32-2432S028R (ESP32 Cheap Yellow Display)的GitHub仓库星标显示器
 * 最终功能版 - 触摸校准修复
 * 
 * ===== 项目概述 =====
 * 这是一个基于ESP32的GitHub仓库信息显示器，能够实时显示指定GitHub仓库的星标数、分支数和关注者数。
 * 项目使用LVGL图形库和LovyanGFX驱动库，提供现代化的触摸界面。
 * 
 * ===== 主要功能特性 =====
 * 1. 实时GitHub数据获取：通过GitHub API获取仓库的星标、分支、关注者数据
 * 2. WiFi连接管理：支持WiFi网络扫描、连接和状态监控
 * 3. 触摸界面操作：完整的触摸界面，支持设置配置和数据查看
 * 4. 数据持久化存储：使用ESP32的NVS存储WiFi和GitHub配置信息
 * 5. 自动更新机制：定时获取最新数据并更新显示
 * 6. 进度条显示：可视化显示数据更新进度和时间间隔
 * 7. 多屏幕导航：设置菜单、WiFi配置、GitHub参数编辑等多个界面
 * 
 * ===== 硬件要求 =====
 * - ESP32-2432S028R开发板（带2.8寸TFT显示屏和触摸功能）
 * - ILI9341显示驱动芯片
 * - XPT2046触摸控制芯片
 * 
 * ===== 软件架构 =====
 * - 显示驱动：LovyanGFX库提供高性能图形渲染
 * - UI框架：LVGL提供现代化的用户界面组件
 * - 网络通信：ESP32 WiFi + HTTPClient进行API调用
 * - 数据解析：ArduinoJson库解析GitHub API响应
 * - 存储管理：Preferences库管理配置数据持久化
 * 
 * ===== 主程序逻辑流程 =====
 * 1. 系统初始化：串口、显示屏、触摸、WiFi模块初始化
 * 2. 配置加载：从NVS读取WiFi和GitHub配置信息
 * 3. UI创建：创建主界面和各个设置界面
 * 4. WiFi连接：尝试连接已保存的WiFi网络
 * 5. 数据获取：连接成功后获取GitHub仓库数据
 * 6. 主循环：处理触摸事件、定时更新数据、监控网络状态
 * 
 * ===== 界面结构 =====
 * - 主界面：显示仓库名称、星标数、分支数、关注者数、状态信息
 * - 设置菜单：WiFi设置、GitHub设置、退出选项
 * - WiFi设置：网络扫描、密码输入、连接管理
 * - GitHub设置：仓库所有者、仓库名称、访问令牌配置
 * 
 * ===== 特殊功能说明 =====
 * 1. 触摸校准：针对ESP32-2432S028R的触摸坐标进行了专门校准
 * 2. 键盘管理：虚拟键盘的显示/隐藏控制，避免界面元素被遮挡
 * 3. 网络状态监控：实时监控WiFi连接状态，自动重连和状态重置
 * 4. 错误处理：完善的错误处理机制，包括网络错误、API错误等
 * 5. 时间显示：支持NTP时间同步和本地时间显示
 */

// ===== 功能开关配置 =====
// 触摸测试功能开关：启用后会在界面左上角显示"T"按钮，用于测试触摸坐标
// 如需禁用触摸测试按钮，请注释下方的宏定义
#define ENABLE_TOUCH_TEST

// ===== 核心库文件引用 =====
#include <Arduino.h>              // Arduino核心库：提供基础的Arduino函数和定义
#include <WiFi.h>                 // ESP32 WiFi库：WiFi连接、扫描、状态管理功能
#include <HTTPClient.h>           // HTTP客户端库：用于向GitHub API发送HTTP请求
#include <WiFiClientSecure.h>     // WiFi安全客户端库：用于HTTPS连接
#include <ArduinoJson.h>          // JSON数据处理库：解析GitHub API返回的JSON格式数据
// 移除gzip解压缩功能，直接处理未压缩响应
#include <LovyanGFX.hpp>          // LovyanGFX图形库：高性能显示驱动，支持ILI9341等显示控制器
#include <lvgl.h>                 // LVGL图形界面库：现代化UI组件库，提供按钮、标签、列表等控件
#include <XPT2046_Touchscreen.h>  // XPT2046触摸屏库：处理电阻式触摸屏的触摸检测和坐标转换
#include <SPI.h>                  // SPI通信库：用于与显示屏和触摸屏进行SPI通信
#include <Preferences.h>          // ESP32偏好设置库：在NVS(非易失性存储)中保存WiFi和GitHub配置
#include <LittleFS.h>             // LittleFS文件系统库：用于持久化存储历史数据
#include "secrets.h"
#include "city_data.h"             // 城市数据头文件：包含拼音城市名称和adcode映射

// ===== Font Awesome 图标字体文件 =====
// 引用Font Awesome字体文件，用于显示各种图标（星星、眼睛、分支、齿轮等）
// 注意：确保"font_awesome_16.cpp"文件与此.ino文件位于同一文件夹中
#include "font_awesome_16.cpp"

// ===== Font Awesome 图标字符定义 =====
// 这些宏定义了Font Awesome图标的UTF-8编码，用于在界面中显示相应图标
#define FA_STAR "\xEF\x80\x85"        // f005 - 星星图标，用于显示GitHub星标数
#define FA_EYE "\xEF\x81\xAE"         // f06e - 眼睛图标，用于显示GitHub关注者数
#define FA_CODE_BRANCH "\xEF\x84\xA6" // f126 - 代码分支图标，用于显示GitHub分支数
#define FA_GEAR "\xEF\x80\x93"        // f013 - 齿轮图标，用于设置按钮

// ===== XPT2046 触摸屏硬件引脚配置 =====
// 这些引脚定义对应ESP32-2432S028R开发板上XPT2046触摸控制器的连接
#define XPT2046_IRQ 36   // 触摸中断引脚：检测到触摸时产生中断信号
#define XPT2046_MOSI 32  // SPI主出从入引脚：ESP32向触摸控制器发送数据
#define XPT2046_MISO 39  // SPI主入从出引脚：ESP32从触摸控制器接收数据
#define XPT2046_CLK 25   // SPI时钟引脚：同步SPI通信的时钟信号
#define XPT2046_CS 33    // SPI片选引脚：选择XPT2046触摸控制器进行通信

// ===== LovyanGFX 显示驱动配置类 =====
// 自定义LGFX类，继承自LovyanGFX设备基类，专门为ESP32-2432S028R配置显示参数
class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ILI9341 _panel_instance;  // ILI9341显示面板实例：2.8寸TFT显示屏控制器
    lgfx::Bus_SPI _bus_instance;          // SPI总线实例：用于ESP32与显示屏的SPI通信
    lgfx::Light_PWM _light_instance;      // PWM背光实例：控制显示屏背光亮度

public:
    // 构造函数：初始化显示驱动的各项配置参数
    LGFX(void) {
        // ===== SPI总线配置 =====
        {
            auto cfg = _bus_instance.config();  // 获取SPI总线配置对象
            cfg.spi_host = HSPI_HOST;           // 使用ESP32的HSPI主机（SPI2）
            cfg.spi_mode = 0;                   // SPI模式0：时钟极性和相位都为0
            cfg.freq_write = 40000000;          // 写入频率：40MHz，提高显示刷新速度
            cfg.freq_read = 16000000;           // 读取频率：16MHz，保证数据读取稳定性
            cfg.spi_3wire = true;               // 启用3线SPI：共用MOSI和MISO线
            cfg.use_lock = true;                // 启用SPI锁：防止多线程冲突
            cfg.dma_channel = SPI_DMA_CH_AUTO;  // 自动选择DMA通道：提高数据传输效率
            cfg.pin_sclk = 14;                  // SPI时钟引脚：GPIO14
            cfg.pin_mosi = 13;                  // SPI主出从入引脚：GPIO13
            cfg.pin_miso = 12;                  // SPI主入从出引脚：GPIO12
            cfg.pin_dc = 2;                     // 数据/命令选择引脚：GPIO2
            _bus_instance.config(cfg);          // 应用SPI总线配置
            _panel_instance.setBus(&_bus_instance);  // 将SPI总线绑定到显示面板
        }
        // ===== 显示面板配置 =====
        {
            auto cfg = _panel_instance.config();  // 获取显示面板配置对象
            cfg.pin_cs = 15;                      // 片选引脚：GPIO15，选择显示控制器
            cfg.pin_rst = -1;                     // 复位引脚：-1表示不使用硬件复位
            cfg.pin_busy = -1;                    // 忙碌引脚：-1表示不使用忙碌检测
            cfg.memory_width = 240;               // 显存宽度：240像素
            cfg.memory_height = 320;              // 显存高度：320像素
            cfg.panel_width = 240;                // 面板实际宽度：240像素
            cfg.panel_height = 320;               // 面板实际高度：320像素
            cfg.offset_x = 0;                     // X轴偏移：0像素
            cfg.offset_y = 0;                     // Y轴偏移：0像素
            cfg.offset_rotation = 0;              // 旋转偏移：0度
            cfg.dummy_read_pixel = 8;             // 虚拟读取像素数：8个
            cfg.dummy_read_bits = 1;              // 虚拟读取位数：1位
            cfg.readable = true;                  // 启用读取功能：允许从显存读取数据
            cfg.invert = false;                   // 颜色反转：false表示正常显示
            cfg.rgb_order = false;                // RGB顺序：false表示RGB，true表示BGR
            cfg.dlen_16bit = false;               // 16位数据长度：false表示使用8位
            cfg.bus_shared = true;               // 共享总线：允许多个设备共用SPI总线
            _panel_instance.config(cfg);         // 应用显示面板配置
        }
        // ===== 背光PWM配置 =====
        {
            auto cfg = _light_instance.config();  // 获取背光配置对象
            cfg.pin_bl = 21;                      // 背光控制引脚：GPIO21
            cfg.invert = false;                   // 背光极性：false表示高电平点亮
            cfg.freq = 44100;                     // PWM频率：44.1kHz，避免可听噪音
            cfg.pwm_channel = 7;                  // PWM通道：使用ESP32的PWM通道7
            _light_instance.config(cfg);          // 应用背光配置
            _panel_instance.setLight(&_light_instance);  // 将背光控制绑定到显示面板
        }
        setPanel(&_panel_instance);              // 将配置好的面板设置为当前设备
    }
};

// ===== 硬件对象实例化 =====
static LGFX tft;                                        // TFT显示屏对象：使用上面配置的LGFX类
SPIClass touchscreenSPI = SPIClass(VSPI);              // 触摸屏SPI对象：使用ESP32的VSPI（SPI3）
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);  // 触摸屏对象：XPT2046控制器
Preferences preferences;                                // NVS偏好设置对象：用于配置数据持久化存储

// ===== LVGL显示缓冲区配置 =====
#define SCREEN_WIDTH 320                               // 屏幕宽度：320像素（横屏模式）
#define SCREEN_HEIGHT 240                              // 屏幕高度：240像素（横屏模式）
#define LVGL_BUFFER_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT / 10)  // LVGL缓冲区大小：屏幕总像素的1/10

// 使用对齐的内存分配来避免LVGL缓冲区对齐错误
static __attribute__((aligned(4))) lv_color_t buf[LVGL_BUFFER_SIZE];   // LVGL主缓冲区：4字节对齐
static __attribute__((aligned(4))) lv_color_t buf2[LVGL_BUFFER_SIZE];  // LVGL副缓冲区：4字节对齐

// ===== 应用程序配置参数 =====
// 这些配置将从ESP32的NVS（非易失性存储）中加载，如果NVS中没有则使用默认值
char ssid[33] = "";                 // WiFi网络名称：最大32字符+结束符
char password[65] = "";             // WiFi密码：最大64字符+结束符
char repoOwner[40] = "lvgl";        // GitHub仓库所有者：可以保留一个示例，如lvgl
char repoName[40] = "lvgl";         // GitHub仓库名称：可以保留一个示例，如lvgl
char githubToken[65] = "";          // GitHub访问令牌：最大64字符+结束符

// ===== 全局状态变量 =====
// GitHub仓库数据状态
int currentStars = -1, currentForks = -1, currentWatchers = -1;  // 当前显示的星标、分支、关注者数量（-1表示未获取）
int yesterdayStars = -1;  // 昨日星标数量（用于计算变化量）
int starsDelta = 0;       // 星标变化量（今日-昨日）

// 时间管理变量
unsigned long lastDataUpdate = 0;        // 上次数据更新时间戳（毫秒）
unsigned long lastTimeUpdate = 0;        // 上次时间显示更新时间戳（毫秒）
unsigned long lastProgressUpdate = 0;    // 上次进度条更新时间戳（毫秒）
unsigned long updateSuccessTime = 0;     // 更新成功消息显示开始时间戳（毫秒）
unsigned long manualRefreshStartTime = 0; // 手动刷新开始时间戳（毫秒）
unsigned long retryTime = 0;             // 重试时间戳（毫秒）

// 状态标志
bool showingUpdateSuccess = false;       // 是否正在显示"Update Successful"消息
bool isFetchingData = false;             // 是否正在获取GitHub数据
bool networkErrorShowing = false;        // 是否正在显示网络错误消息框
bool isManualRefreshing = false;         // 是否正在手动刷新
bool refreshButtonGreen = false;         // 刷新按钮是否为绿色状态
bool waitingForRetry = false;            // 是否正在等待重试

// 数字动画相关变量
int animatingStars = -1;                 // 正在动画的星标数值
int animatingForks = -1;                 // 正在动画的分支数值
int animatingWatchers = -1;              // 正在动画的关注者数值

// 时间间隔常量
const unsigned long UPDATE_INTERVAL = 300000;      // 数据更新间隔：5分钟（300秒）
const unsigned long TIME_UPDATE_INTERVAL = 60000;  // 时间显示更新间隔：1分钟（60秒）
const unsigned long SUCCESS_DISPLAY_TIME = 5000;   // 成功消息显示时长：5秒
const unsigned long MANUAL_REFRESH_DURATION = 5000; // 手动刷新倒计时时长：5秒
const unsigned long RETRY_INTERVAL = 1000;        // 重试间隔：1秒

// ===== LVGL屏幕对象声明 =====
// 主要界面屏幕
lv_obj_t *main_screen;              // 主界面：显示GitHub仓库信息
lv_obj_t *screen_settings;          // 设置主菜单界面
lv_obj_t *screen_wifi_list;         // WiFi网络列表界面
lv_obj_t *screen_wifi_password;     // WiFi密码输入界面
lv_obj_t *screen_github_settings;   // GitHub设置主界面

// GitHub参数编辑界面
lv_obj_t *screen_edit_owner;        // 编辑仓库所有者界面
lv_obj_t *screen_edit_repo;         // 编辑仓库名称界面
lv_obj_t *screen_edit_token;        // 编辑GitHub令牌界面
lv_obj_t *screen_city_input;        // 城市输入界面

// 数据可视化界面
lv_obj_t *screen_chart;             // 数据可视化图表界面
lv_obj_t *screen_weather;           // 天气信息界面
lv_obj_t *screen_calendar;          // 日历界面
// ===== 主界面UI组件对象 =====
lv_obj_t *title_label;          // 标题标签：显示仓库名称（所有者/仓库名）
lv_obj_t *current_time_label;   // 当前时间标签：显示实时时间
lv_obj_t *stars_count_label;    // 星标数量标签：显示GitHub星标数
lv_obj_t *stars_delta_label;    // 星标变化量标签：显示今日vs昨日的变化（+/-数字）
lv_obj_t *forks_label;          // 分支数量标签：显示GitHub分支数
lv_obj_t *watchers_label;       // 关注者数量标签：显示GitHub关注者数
lv_obj_t *status_label;         // 状态标签：显示连接状态、错误信息等
lv_obj_t *time_label;           // 时间标签：显示最后更新时间或"Go to Settings"
lv_obj_t *progress_bar;         // 进度条：显示距离下次更新的时间进度

// ===== 通用UI组件对象 =====
lv_obj_t *kb;                   // 虚拟键盘：用于文本输入（WiFi密码、GitHub参数等）
lv_obj_t *settings_btn;         // 设置按钮：右上角齿轮图标，点击进入设置菜单
lv_obj_t *touch_test_btn;       // 触摸测试按钮：左上角"T"图标，用于测试触摸坐标

// ===== 天气界面显示元素引用 =====
lv_obj_t *weather_location_label;  // 天气位置显示标签
lv_obj_t *weather_temp_label;      // 天气温度显示标签
lv_obj_t *weather_condition_label; // 天气状况显示标签
lv_obj_t *weather_humidity_label;  // 湿度显示标签
lv_obj_t *weather_wind_label;      // 风力等级显示标签

// ===== 页面指示器相关变量 =====
lv_obj_t *page_indicator_main;     // 主页面指示器容器
lv_obj_t *page_indicator_weather;  // 天气页面指示器容器
lv_obj_t *page_indicator_calendar; // 日历页面指示器容器
lv_obj_t *indicator_dots[3];       // 指示器小圆点数组（0=天气，1=主页，2=日历）

// ===== 临时数据存储 =====
static char selected_ssid[33];  // 当前选中的WiFi网络名称：用于WiFi连接流程

// ===== 数据可视化相关变量 =====
lv_obj_t *chart_obj;                    // LVGL图表对象
lv_chart_series_t *chart_series;        // 图表数据系列
static lv_coord_t chart_data[50];       // 图表数据数组（最多50个数据点）
int chart_data_count = 0;               // 当前数据点数量
int chart_view_mode = 0;                // 图表显示模式：0=每次获取，1=每天数据，2=增长数据
const char* DATA_FILE = "/stardata.csv"; // 数据文件路径
const char* DAILY_DATA_FILE = "/daily_stars.csv"; // 每日数据文件路径
const char* GROWTH_DATA_FILE = "/growth_data.csv"; // 增长数据文件路径

// ===== 数据持久化相关变量 =====
int lastSavedStars = -1;                // 上次保存的星标数，用于检测增长
time_t lastDailySaveTime = 0;           // 上次每日保存的时间戳

// ===== 天气数据相关变量 =====
float currentTemperature = 0.0;         // 当前温度（摄氏度）
int currentHumidity = 0;                // 当前湿度（百分比）
String currentWindPower = "";           // 当前风力等级
String currentWeatherCondition = "Unknown"; // 当前天气状况
String currentLocation = "Unknown";     // 当前位置
unsigned long lastWeatherUpdate = 0;    // 上次天气更新时间戳（毫秒）
bool isWeatherDataValid = false;        // 天气数据是否有效
const unsigned long WEATHER_UPDATE_INTERVAL = 600000; // 天气更新间隔：10分钟

// 高德天气API配置
const char* WEATHER_API_HOST = "restapi.amap.com";
const char* WEATHER_API_KEY = SECRET_WEATHER_API_KEY;  // 高德地图API密钥（从secrets.h引用）
const char* DEFAULT_LOCATION = "110101";  // 默认位置ID（北京东城区）
String userLocation = DEFAULT_LOCATION;  // 用户设置的位置ID

// ===== 函数前置声明 =====
// UI创建函数
void createUI();                                              // 创建主界面UI
void create_settings_screen();                                // 创建设置主菜单界面
void create_wifi_list_screen();                               // 创建WiFi网络列表界面
void create_wifi_password_screen(const char *ssid_name);      // 创建WiFi密码输入界面
void create_github_settings_screen();                         // 创建GitHub设置主界面
void create_edit_owner_screen();                              // 创建编辑仓库所有者界面
void create_edit_repo_screen();                               // 创建编辑仓库名称界面
void create_edit_token_screen();                              // 创建编辑GitHub令牌界面
void create_city_input_screen();                              // 创建城市输入界面
void create_chart_screen();                                   // 创建数据可视化图表界面
void create_weather_screen();                                 // 创建天气信息界面
void create_calendar_screen();                                // 创建日历界面

// 天气数据管理函数
void fetchWeatherData();                                      // 获取天气数据
void updateWeatherDisplay();                                  // 更新天气显示
void loadWeatherSettings();                                   // 加载天气设置
void saveWeatherSettings();                                   // 保存天气设置
// getWeatherDescription函数已删除，高德天气API直接提供天气描述

// 页面指示器管理函数
void createPageIndicator(lv_obj_t* parent);                  // 创建页面指示器
void updatePageIndicator(int current_page);                  // 更新页面指示器状态（0=天气，1=主页，2=日历）

// 数据管理函数
void load_settings();                                         // 从NVS加载配置设置
void save_settings();                                         // 将配置设置保存到NVS
void fetchGitHubData();                                       // 获取GitHub仓库数据
void saveStarData(int starCount);                             // 保存星标数据到文件
void saveDailyStarData(int starCount);                        // 保存每日星标数据
void saveGrowthData(int starCount);                           // 保存增长数据
void checkAndSaveDailyData();                                 // 检查并保存每日数据（23:59调用）
void loadChartData();                                         // 从文件加载图表数据
void updateChartDisplay();                                    // 更新图表显示

// 显示更新函数
void updateStatus(const char *message, lv_color_t color);     // 更新状态标签显示
void showCurrentTime();                                       // 显示当前时间
void updateCurrentTimeDisplay();                              // 更新当前时间显示
void updateTimeDisplay();                                     // 更新时间标签显示
void updateProgressBar();                                     // 更新进度条显示

// 数字动画函数
void animateNumber(lv_obj_t* label, int from, int to, const char* prefix); // 数字滚动动画
static void number_anim_cb(void* var, int32_t val);          // 数字动画回调函数

// 事件回调函数
static void hide_keyboard_event_cb(lv_event_t * e);          // 隐藏虚拟键盘事件回调
static void show_keyboard_event_cb(lv_event_t * e);          // 显示虚拟键盘事件回调
void city_input_ta_event_cb(lv_event_t * e);                 // 城市输入文本框事件回调
static void edit_field_event_cb(lv_event_t * e);             // 编辑字段事件回调
static void save_field_event_cb(lv_event_t * e);             // 保存字段事件回调
static void stars_card_event_cb(lv_event_t * e);             // 星星卡片点击事件回调
static void chart_back_event_cb(lv_event_t * e);             // 图表界面返回事件回调
static void chart_mode_event_cb(lv_event_t * e);             // 图表模式切换事件回调

// ===== LVGL核心驱动函数 =====

/**
 * LVGL显示刷新回调函数
 * 功能：将LVGL渲染的图像数据传输到物理显示屏
 * 参数：
 *   - disp: LVGL显示设备对象
 *   - area: 需要刷新的屏幕区域
 *   - px_map: 像素数据缓冲区
 */
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    uint32_t w = (area->x2 - area->x1 + 1);  // 计算刷新区域宽度
    uint32_t h = (area->y2 - area->y1 + 1);  // 计算刷新区域高度
    if (tft.getStartCount() == 0) tft.startWrite();  // 开始SPI传输（如果尚未开始）
    tft.pushImage(area->x1, area->y1, w, h, (lgfx::rgb565_t *)px_map);  // 将像素数据推送到显示屏
    lv_display_flush_ready(disp);  // 通知LVGL刷新完成
}

/**
 * LVGL触摸输入读取回调函数
 * 功能：读取触摸屏状态并转换为LVGL坐标系统
 * 参数：
 *   - indev_driver: LVGL输入设备驱动对象
 *   - data: 触摸数据输出结构体
 */
void my_touchpad_read(lv_indev_t *indev_driver, lv_indev_data_t *data) {
    // 检查触摸中断引脚和触摸状态
    if (touchscreen.tirqTouched() && touchscreen.touched()) {
        TS_Point p = touchscreen.getPoint();  // 获取原始触摸坐标
        
        // 坐标映射：将XPT2046的原始坐标转换为屏幕坐标
        // 针对ESP32-2432S028R的触摸校准参数
        int mapped_x = map(p.x, 200, 3700, SCREEN_WIDTH - 1, 0);   // X轴映射
        int mapped_y = map(p.y, 240, 3800, SCREEN_HEIGHT - 1, 0);  // Y轴映射

        // 坐标约束：确保坐标值在屏幕范围内，防止越界
        data->point.x = constrain(mapped_x, 0, SCREEN_WIDTH - 1);
        data->point.y = constrain(mapped_y, 0, SCREEN_HEIGHT - 1);
        data->state = LV_INDEV_STATE_PR;  // 设置触摸状态为按下
    } else {
        data->state = LV_INDEV_STATE_REL;  // 设置触摸状态为释放（未触摸）
    }
}

// ===== UI事件处理函数 =====

/**
 * 模态对话框关闭事件回调函数
 * 功能：处理模态对话框的关闭操作，删除对话框对象
 * 参数：e - LVGL事件对象，用户数据包含要删除的模态背景对象
 */
static void modal_close_event_cb(lv_event_t * e) {
    lv_obj_t * modal_bg = (lv_obj_t *)lv_event_get_user_data(e);  // 获取模态背景对象
    lv_obj_del(modal_bg);  // 删除模态对话框及其所有子对象
}

/**
 * 模态对话框关闭并跳转到设置界面的事件回调函数
 * 功能：关闭模态对话框并跳转到设置界面
 * 参数：e - LVGL事件对象，用户数据包含模态背景对象
 */
static void modal_close_and_goto_settings_cb(lv_event_t * e) {
    lv_obj_t * modal_bg = (lv_obj_t *)lv_event_get_user_data(e);  // 获取模态背景对象
    lv_obj_del(modal_bg);  // 删除模态对话框及其所有子对象
    // 跳转到设置界面
    lv_scr_load_anim(screen_settings, LV_SCR_LOAD_ANIM_MOVE_LEFT, 100, 0, false);
}

/**
 * 返回按钮通用事件回调函数
 * 功能：处理各个界面的返回操作，切换到指定的目标屏幕
 * 参数：e - LVGL事件对象，用户数据包含要切换到的目标屏幕对象
 */
static void back_button_event_cb(lv_event_t * e) {
    lv_scr_load_anim((lv_obj_t*)lv_event_get_user_data(e), LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, false);  // 使用右滑动画加载目标屏幕
}

/**
 * 显示自定义消息框函数
 * 功能：创建并显示一个模态消息对话框，用于显示提示信息、错误信息等
 * 参数：
 *   - title: 对话框标题文本
 *   - message: 对话框内容文本
 * 特点：使用半透明背景遮罩，支持文本自动换行，居中显示
 */
static void show_message_box(const char* title, const char* message) {
    // 创建半透明遮罩层，覆盖整个屏幕
    lv_obj_t* shader = lv_obj_create(lv_layer_top());  // 在顶层创建遮罩
    lv_obj_set_size(shader, LV_PCT(100), LV_PCT(100));  // 设置为全屏大小
    lv_obj_set_style_bg_color(shader, lv_color_black(), 0);  // 设置黑色背景
    lv_obj_set_style_bg_opa(shader, LV_OPA_50, 0);  // 设置50%透明度
    lv_obj_set_style_border_width(shader, 0, 0);  // 移除边框

    // 创建消息框主体
    lv_obj_t* mbox = lv_obj_create(shader);  // 在遮罩上创建消息框
    lv_obj_set_size(mbox, 260, 150);  // 设置消息框尺寸
    lv_obj_center(mbox);  // 居中显示
    lv_obj_set_style_radius(mbox, 10, 0);  // 设置圆角半径

    // 创建标题标签
    lv_obj_t* title_label = lv_label_create(mbox);  // 在消息框中创建标题
    lv_label_set_text(title_label, title);  // 设置标题文本
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_16, 0);  // 设置字体
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 10, 10);  // 左上角对齐

    // 创建内容标签
    lv_obj_t* text_label = lv_label_create(mbox);  // 在消息框中创建内容
    lv_label_set_text(text_label, message);  // 设置内容文本
    lv_obj_set_width(text_label, 240);  // 设置标签宽度
    lv_label_set_long_mode(text_label, LV_LABEL_LONG_WRAP);  // 启用文本自动换行
    lv_obj_align_to(text_label, title_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);  // 相对标题定位

    // 创建确定按钮
    lv_obj_t* ok_btn = lv_btn_create(mbox);  // 在消息框中创建按钮
    lv_obj_set_size(ok_btn, 100, 40);  // 设置按钮尺寸
    lv_obj_align(ok_btn, LV_ALIGN_BOTTOM_MID, 0, -10);  // 底部居中对齐
    lv_obj_add_event_cb(ok_btn, modal_close_event_cb, LV_EVENT_CLICKED, shader);  // 绑定关闭事件

    // 创建按钮标签
    lv_obj_t* btn_label = lv_label_create(ok_btn);  // 在按钮上创建文本标签
    lv_label_set_text(btn_label, "Ok");  // 设置按钮文本
    lv_obj_center(btn_label);  // 居中显示文本
}

/**
 * 显示网络错误消息框并引导到设置界面
 * 功能：创建并显示一个模态消息对话框，点击OK后跳转到设置界面
 * 参数：
 *   - title: 对话框标题文本
 *   - message: 对话框内容文本
 * 特点：使用半透明背景遮罩，支持文本自动换行，点击OK跳转到设置界面
 */
static void show_network_error_message_box(const char* title, const char* message) {
    // 如果已经在显示错误消息框，则不重复显示
    if (networkErrorShowing) {
        return;
    }
    networkErrorShowing = true;  // 设置错误状态标志
    
    // 创建半透明遮罩层，覆盖整个屏幕
    lv_obj_t* shader = lv_obj_create(lv_layer_top());  // 在顶层创建遮罩
    lv_obj_set_size(shader, LV_PCT(100), LV_PCT(100));  // 设置为全屏大小
    lv_obj_set_style_bg_color(shader, lv_color_black(), 0);  // 设置黑色背景
    lv_obj_set_style_bg_opa(shader, LV_OPA_50, 0);  // 设置50%透明度
    lv_obj_set_style_border_width(shader, 0, 0);  // 移除边框

    // 创建消息框主体（增大高度以容纳更多文字）
    lv_obj_t* mbox = lv_obj_create(shader);  // 在遮罩上创建消息框
    lv_obj_set_size(mbox, 260, 120);  // 设置消息框尺寸，减小高度为按钮留出空间
    lv_obj_align(mbox, LV_ALIGN_CENTER, 0, -25);  // 向上偏移25像素，为按钮留出空间
    lv_obj_set_style_radius(mbox, 10, 0);  // 设置圆角半径

    // 创建标题标签
    lv_obj_t* title_label = lv_label_create(mbox);  // 在消息框中创建标题
    lv_label_set_text(title_label, title);  // 设置标题文本
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_16, 0);  // 设置字体
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 10, 10);  // 左上角对齐

    // 创建内容标签
    lv_obj_t* text_label = lv_label_create(mbox);  // 在消息框中创建内容
    lv_label_set_text(text_label, message);  // 设置内容文本
    lv_obj_set_width(text_label, 240);  // 设置标签宽度
    lv_label_set_long_mode(text_label, LV_LABEL_LONG_WRAP);  // 启用文本自动换行
    lv_obj_align_to(text_label, title_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);  // 相对标题定位

    // 创建确定按钮（放在消息框外部）
    lv_obj_t* ok_btn = lv_btn_create(shader);  // 在遮罩层上直接创建按钮
    lv_obj_set_size(ok_btn, 100, 40);  // 设置按钮尺寸
    lv_obj_align_to(ok_btn, mbox, LV_ALIGN_OUT_BOTTOM_MID, 0, 15);  // 相对消息框底部居中，向下偏移15像素
    lv_obj_add_event_cb(ok_btn, modal_close_and_goto_settings_cb, LV_EVENT_CLICKED, shader);  // 绑定跳转到设置的事件

    // 创建按钮标签
    lv_obj_t* btn_label = lv_label_create(ok_btn);  // 在按钮上创建文本标签
    lv_label_set_text(btn_label, "OK");  // 设置按钮文本
    lv_obj_center(btn_label);  // 居中显示文本
}

/**
 * 按钮显示控制函数
 * 功能：根据当前显示的屏幕控制功能按钮的显示/隐藏状态
 * 参数：current_screen - 当前活动的屏幕对象
 * 逻辑：只在主屏幕显示设置按钮和触摸测试按钮，其他屏幕隐藏这些按钮
 */
void control_buttons_visibility(lv_obj_t* current_screen) {
    if (current_screen == main_screen) {
        // 在主屏幕显示功能按钮
        if (settings_btn) lv_obj_clear_flag(settings_btn, LV_OBJ_FLAG_HIDDEN);  // 显示设置按钮
#ifdef ENABLE_TOUCH_TEST
        if (touch_test_btn) lv_obj_clear_flag(touch_test_btn, LV_OBJ_FLAG_HIDDEN);  // 显示触摸测试按钮
#endif
    } else {
        // 在其他屏幕隐藏功能按钮，避免界面混乱
        if (settings_btn) lv_obj_add_flag(settings_btn, LV_OBJ_FLAG_HIDDEN);  // 隐藏设置按钮
#ifdef ENABLE_TOUCH_TEST
        if (touch_test_btn) lv_obj_add_flag(touch_test_btn, LV_OBJ_FLAG_HIDDEN);  // 隐藏触摸测试按钮
#endif
    }
}

/**
 * 设置按钮点击事件回调函数
 * 功能：处理设置按钮（齿轮图标）的点击事件，跳转到设置主菜单
 * 参数：e - LVGL事件对象
 */
static void settings_button_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);  // 获取事件类型
    lv_obj_t * btn = (lv_obj_t *)lv_event_get_target(e);  // 获取触发事件的按钮对象
    if (code == LV_EVENT_CLICKED) {  // 检查是否为点击事件
        Serial.println("设置按钮已被点击！正在跳转到设置界面...");  // 调试输出
        lv_scr_load_anim(screen_settings, LV_SCR_LOAD_ANIM_MOVE_LEFT, 100, 0, false);  // 使用左滑动画切换到设置主菜单屏幕
        control_buttons_visibility(screen_settings);  // 更新按钮显示状态
    }
}

/**
 * 设置菜单列表项点击事件回调函数
 * 功能：处理设置主菜单中各个选项的点击事件（WiFi设置、GitHub设置、退出）
 * 参数：e - LVGL事件对象
 */
static void settings_list_event_cb(lv_event_t * e) {
    // 静态变量声明，用于保存WiFi扫描前的连接信息
    static String currentSSID = "";
    static String currentPassword = "";
    static bool wasConnected = false;
    
    lv_event_code_t code = lv_event_get_code(e);  // 获取事件类型
    lv_obj_t * obj = (lv_obj_t*)lv_event_get_target(e);  // 获取触发事件的按钮对象
    if(code == LV_EVENT_CLICKED) {  // 检查是否为点击事件
        // 通过按钮内的标签文本来识别按钮类型
        lv_obj_t* label = lv_obj_get_child(obj, 0);  // 获取按钮内的标签
        const char * txt = lv_label_get_text(label);  // 获取标签文本
        
        if (strstr(txt, "WiFi Setup") != NULL) {  // 如果点击的是WiFi设置按钮
            Serial.println("=== 开始WiFi设置 ===");  // 调试输出
            lv_scr_load_anim(screen_wifi_list, LV_SCR_LOAD_ANIM_MOVE_BOTTOM, 100, 0, false);  // 使用向下滑动画切换到WiFi列表屏幕
            control_buttons_visibility(screen_wifi_list);  // 更新按钮显示状态
            lv_obj_t* list = lv_obj_get_child(screen_wifi_list, 1);  // 获取WiFi列表对象
            lv_obj_clean(list);  // 清空列表内容
            lv_obj_t* label = lv_obj_get_child(screen_wifi_list, 2);  // 获取状态标签
            lv_label_set_text(label, "Scanning for WiFi...");  // 显示扫描状态
            lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);  // 确保状态标签可见
            
            // WiFi扫描流程开始
            Serial.println("开始WiFi扫描...");  // 调试输出
            Serial.printf("WiFi模式: %d\n", WiFi.getMode());  // 输出当前WiFi模式
            Serial.printf("WiFi状态: %d\n", WiFi.status());  // 输出当前WiFi状态
            
            // 保存当前WiFi连接信息
            currentSSID = WiFi.SSID();
            currentPassword = "";
            wasConnected = (WiFi.status() == WL_CONNECTED);
            
            // 如果当前已连接，保存密码信息（从preferences读取）
            if (wasConnected) {
                preferences.begin("wifi", true);
                currentPassword = preferences.getString("password", "");
                preferences.end();
                Serial.printf("保存当前WiFi信息: %s\n", currentSSID.c_str());
            }
            
            // 确保WiFi处于Station模式，但不断开当前连接
            if (WiFi.getMode() != WIFI_STA && WiFi.getMode() != WIFI_AP_STA) {
                WiFi.mode(WIFI_STA);  // 设置为Station模式
                delay(100);  // 等待模式切换完成
            }
            
            Serial.println("开始WiFi扫描（保持当前连接）...");
            int scan_result = WiFi.scanNetworks(true);  // 启动异步WiFi网络扫描（不断开当前连接）
            Serial.printf("扫描启动结果: %d\n", scan_result);
            
            // 处理扫描失败的情况
            if (scan_result == -2) {  // -2表示扫描失败
                 Serial.println("扫描失败，等待后重试...");
                 delay(1000);  // 等待1秒后重试
                 scan_result = WiFi.scanNetworks(true);  // 重试扫描（不断开连接）
                 Serial.printf("重试扫描结果: %d\n", scan_result);
                 
                 // 如果重试仍然失败，执行强制WiFi模块重置
                 if (scan_result == -2) {
                     Serial.println("WiFi扫描仍然失败，开始强制重置WiFi模块...");
                     
                     // 保存当前连接状态
                     bool was_connected = (WiFi.status() == WL_CONNECTED);
                     String current_ssid = "";
                     if (was_connected) {
                         current_ssid = WiFi.SSID();
                         Serial.printf("保存当前连接: %s\n", current_ssid.c_str());
                     }
                     
                     // 完全重置WiFi模块
                     Serial.println("步骤1: 断开所有WiFi连接...");
                     WiFi.disconnect(true);  // 完全断开并清除配置
                     delay(500);
                     
                     Serial.println("步骤2: 关闭WiFi模块...");
                     WiFi.mode(WIFI_OFF);    // 关闭WiFi
                     delay(1000);
                     
                     Serial.println("步骤3: 重新启用WiFi Station模式...");
                     WiFi.mode(WIFI_STA);    // 重新启用Station模式
                     delay(1000);
                     
                     // 如果之前有连接，尝试恢复连接
                     if (was_connected && current_ssid.length() > 0) {
                         Serial.printf("步骤4: 尝试恢复连接到 %s...\n", current_ssid.c_str());
                         WiFi.begin(current_ssid, currentPassword);  // 使用保存的凭据重新连接
                         
                         // 等待连接恢复（最多10秒）
                         int reconnect_attempts = 0;
                         while (WiFi.status() != WL_CONNECTED && reconnect_attempts < 20) {
                             delay(500);
                             reconnect_attempts++;
                             Serial.print(".");
                         }
                         
                         if (WiFi.status() == WL_CONNECTED) {
                             Serial.println("\nWiFi连接已恢复");
                         } else {
                             Serial.println("\nWiFi连接恢复失败");
                         }
                     }
                     
                     Serial.println("步骤5: 重置后重新尝试扫描...");
                     delay(500);
                     scan_result = WiFi.scanNetworks(true);  // 重置后重新扫描
                     Serial.printf("重置后扫描结果: %d\n", scan_result);
                     
                     // 最终检查扫描结果
                     if (scan_result == -2) {
                         Serial.println("WiFi模块重置后扫描仍然失败，可能存在硬件问题");
                         lv_obj_t* label = lv_obj_get_child(screen_wifi_list, 2);  // 获取状态标签
                         lv_label_set_text(label, "WiFi module reset failed.\nPlease restart device.");  // 显示错误信息
                     } else {
                         Serial.println("WiFi模块重置成功，扫描功能已恢复");
                         lv_obj_t* label = lv_obj_get_child(screen_wifi_list, 2);  // 获取状态标签
                         lv_label_set_text(label, "WiFi module reset. Scanning...");  // 显示重置成功信息
                     }
                 }
             }
        } else if (strstr(txt, "GitHub Setup") != NULL) {  // 如果点击的是GitHub设置按钮
            lv_scr_load_anim(screen_github_settings, LV_SCR_LOAD_ANIM_MOVE_BOTTOM, 100, 0, false);  // 使用向下滑动画切换到GitHub设置屏幕
            control_buttons_visibility(screen_github_settings);  // 更新按钮显示状态
        } else if (strstr(txt, "Exit") != NULL) {  // 如果点击的是退出按钮
            lv_scr_load_anim(main_screen, LV_SCR_LOAD_ANIM_FADE_ON, 100, 0, false);  // 使用淡入动画返回主屏幕
            control_buttons_visibility(main_screen);  // 更新按钮显示状态
        }
    }
}

/**
 * 创建页面指示器函数
 * 功能：在指定的父容器中创建三个小圆点作为页面指示器
 * 参数：parent - 父容器对象
 */
void createPageIndicator(lv_obj_t* parent) {
    // 创建指示器容器
    lv_obj_t* indicator_container = lv_obj_create(parent);
    lv_obj_set_size(indicator_container, 80, 20);
    lv_obj_align(indicator_container, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_style_bg_opa(indicator_container, LV_OPA_TRANSP, 0);  // 透明背景
    lv_obj_set_style_border_width(indicator_container, 0, 0);  // 无边框
    lv_obj_clear_flag(indicator_container, LV_OBJ_FLAG_SCROLLABLE);
    
    // 创建三个小圆点
    for (int i = 0; i < 3; i++) {
        indicator_dots[i] = lv_obj_create(indicator_container);
        lv_obj_set_size(indicator_dots[i], 8, 8);
        lv_obj_set_style_radius(indicator_dots[i], LV_RADIUS_CIRCLE, 0);  // 圆形
        lv_obj_set_style_border_width(indicator_dots[i], 0, 0);  // 无边框
        lv_obj_clear_flag(indicator_dots[i], LV_OBJ_FLAG_SCROLLABLE);
        
        // 设置位置：左中右排列
        lv_obj_align(indicator_dots[i], LV_ALIGN_CENTER, (i - 1) * 20, 0);
        
        // 设置默认颜色（非活跃状态）
        lv_obj_set_style_bg_color(indicator_dots[i], lv_color_hex(0x4b5563), 0);
    }
    
    // 保存指示器容器引用
    if (parent == main_screen) {
        page_indicator_main = indicator_container;
    } else if (parent == screen_weather) {
        page_indicator_weather = indicator_container;
    } else if (parent == screen_calendar) {
        page_indicator_calendar = indicator_container;
    }
}

/**
 * 更新页面指示器状态函数
 * 功能：根据当前页面更新指示器小圆点的颜色
 * 参数：current_page - 当前页面索引（0=天气，1=主页，2=日历）
 */
void updatePageIndicator(int current_page) {
    for (int i = 0; i < 3; i++) {
        if (indicator_dots[i] != NULL) {
            if (i == current_page) {
                // 当前页面：亮色显示
                lv_obj_set_style_bg_color(indicator_dots[i], lv_color_hex(0xfbbf24), 0);
            } else {
                // 非当前页面：暗色显示
                lv_obj_set_style_bg_color(indicator_dots[i], lv_color_hex(0x4b5563), 0);
            }
        }
    }
}

/**
 * 创建数据可视化图表界面
 * 功能：创建包含图表、返回按钮和模式切换按钮的界面
 */
void create_chart_screen() {
    Serial.println("=== 创建图表界面 ===");
    
    // 创建图表屏幕
    screen_chart = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_chart, lv_color_hex(0x0f172a), 0);  // 深色背景
    
    // 创建标题标签
    lv_obj_t* title_label = lv_label_create(screen_chart);
    lv_label_set_text(title_label, "STARS Chart");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0xf1f5f9), 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 10);
    
    // 创建返回按钮
    lv_obj_t* back_btn = lv_btn_create(screen_chart);
    lv_obj_set_size(back_btn, 60, 30);
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x475569), 0);
    lv_obj_set_style_bg_grad_color(back_btn, lv_color_hex(0x64748b), 0);
    lv_obj_set_style_bg_grad_dir(back_btn, LV_GRAD_DIR_VER, 0);
    lv_obj_add_event_cb(back_btn, chart_back_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t* back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Back");
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(back_label, lv_color_hex(0xf1f5f9), 0);
    lv_obj_center(back_label);
    
    // 创建模式切换按钮
    lv_obj_t* mode_btn = lv_btn_create(screen_chart);
    lv_obj_set_size(mode_btn, 80, 30);
    lv_obj_align(mode_btn, LV_ALIGN_TOP_RIGHT, -10, 10);
    lv_obj_set_style_bg_color(mode_btn, lv_color_hex(0x3b82f6), 0);
    lv_obj_set_style_bg_grad_color(mode_btn, lv_color_hex(0x2563eb), 0);
    lv_obj_set_style_bg_grad_dir(mode_btn, LV_GRAD_DIR_VER, 0);
    lv_obj_add_event_cb(mode_btn, chart_mode_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t* mode_label = lv_label_create(mode_btn);
    lv_label_set_text(mode_label, "Each Fetch");
    lv_obj_set_style_text_font(mode_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(mode_label, lv_color_hex(0xffffff), 0);
    lv_obj_center(mode_label);
    
    // 创建图表容器
    lv_obj_t* chart_container = lv_obj_create(screen_chart);
    lv_obj_set_size(chart_container, 300, 180);
    lv_obj_align(chart_container, LV_ALIGN_CENTER, 0, 25);
    lv_obj_set_style_bg_color(chart_container, lv_color_hex(0x1e293b), 0);
    lv_obj_set_style_border_color(chart_container, lv_color_hex(0x475569), 0);
    lv_obj_set_style_border_width(chart_container, 1, 0);
    lv_obj_set_style_radius(chart_container, 8, 0);
    lv_obj_set_style_pad_all(chart_container, 10, 0);
    
    // 禁用图表容器的滚动功能
    lv_obj_clear_flag(chart_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(chart_container, LV_SCROLLBAR_MODE_OFF);
    
    // 创建LVGL图表对象
    chart_obj = lv_chart_create(chart_container);
    lv_obj_set_size(chart_obj, 260, 140);  // 减小图表尺寸，为标签留出空间
    lv_obj_center(chart_obj);
    lv_chart_set_type(chart_obj, LV_CHART_TYPE_LINE);   // 设置为折线图
    lv_chart_set_point_count(chart_obj, 10);           // 最多显示50个数据点
    lv_chart_set_range(chart_obj, LV_CHART_AXIS_PRIMARY_Y, 0, 100);  // 初始Y轴范围
    
    // 禁用图表滚动功能和滚动条
    lv_obj_clear_flag(chart_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(chart_obj, LV_SCROLLBAR_MODE_OFF);
    
    // 设置图表样式
    lv_obj_set_style_bg_color(chart_obj, lv_color_hex(0x0f172a), 0);
    lv_obj_set_style_border_color(chart_obj, lv_color_hex(0x475569), 0);
    lv_obj_set_style_border_width(chart_obj, 1, 0);
    
    // 设置网格线样式
    lv_obj_set_style_line_color(chart_obj, lv_color_hex(0x374151), LV_PART_ITEMS);
    lv_obj_set_style_line_width(chart_obj, 1, LV_PART_ITEMS);
    
    // 创建数据系列
    chart_series = lv_chart_add_series(chart_obj, lv_color_hex(0x10b981), LV_CHART_AXIS_PRIMARY_Y);
    
    // 设置数据点样式
    lv_obj_set_style_line_width(chart_obj, 3, LV_PART_ITEMS);
    lv_obj_set_style_size(chart_obj, 6, 6, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(chart_obj, lv_color_hex(0x10b981), LV_PART_INDICATOR);
    
    // 创建Y轴标签（纵轴）- 放在图表左上角，竖向排列
    lv_obj_t* y_axis_label = lv_label_create(chart_container);
    lv_label_set_text(y_axis_label, "STARS");
    lv_obj_set_style_text_font(y_axis_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(y_axis_label, lv_color_hex(0x94a3b8), 0);
    lv_obj_align_to(y_axis_label, chart_obj, LV_ALIGN_OUT_TOP_LEFT, -5, -5);
    lv_obj_set_style_transform_angle(y_axis_label, 900, 0);  // 旋转90度
    
    // 移除TIME标签，直接在图表下方显示横坐标标签
    
    // 说明文本已删除，改为在数据点上显示具体数值
    
    Serial.println("图表界面创建完成");
}

/**
 * WiFi列表项点击事件处理函数
 * 功能：处理用户点击WiFi网络列表中某个网络的事件
 * 参数：e - LVGL事件对象
 * 特点：
 * - 获取用户选择的WiFi网络名称(SSID)
 * - 保存选择的SSID到全局变量
 * - 创建并切换到WiFi密码输入界面
 */
static void wifi_list_event_cb(lv_event_t * e) {
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);  // 获取被点击的按钮对象
    const char * ssid_name = lv_list_get_btn_text(lv_obj_get_parent(btn), btn);  // 获取按钮上的SSID文本
    strncpy(selected_ssid, ssid_name, sizeof(selected_ssid) - 1);  // 保存选择的SSID
    create_wifi_password_screen(ssid_name);  // 创建WiFi密码输入界面
    lv_scr_load_anim(screen_wifi_password, LV_SCR_LOAD_ANIM_MOVE_TOP, 100, 0, false);  // 使用向上滑动画切换到密码输入屏幕
    control_buttons_visibility(screen_wifi_password);  // 更新按钮显示状态
}

/**
 * WiFi连接事件处理函数
 * 功能：处理用户点击连接按钮后的WiFi连接流程
 * 参数：e - LVGL事件对象，用户数据包含密码输入框对象
 * 特点：
 * - 获取用户输入的WiFi密码
 * - 执行WiFi连接流程
 * - 显示连接状态和结果
 * - 包含连接超时和错误处理
 */
static void wifi_connect_event_cb(lv_event_t * e) {
    lv_obj_t* pwd_ta = (lv_obj_t*)lv_event_get_user_data(e);  // 获取密码输入框对象
    const char* pwd = lv_textarea_get_text(pwd_ta);  // 获取用户输入的密码

    // 调试信息输出
    Serial.println("=== 用户触发WiFi连接 ===");
    Serial.print("选择的SSID: "); Serial.println(selected_ssid);
    Serial.print("输入的密码长度: "); Serial.println(strlen(pwd));
    
    // 保存WiFi凭据到全局变量
    strncpy(ssid, selected_ssid, sizeof(ssid) - 1);  // 复制SSID
    ssid[sizeof(ssid) - 1] = '\0';  // 确保字符串结束
    strncpy(password, pwd, sizeof(password) - 1);  // 复制密码
    password[sizeof(password) - 1] = '\0';  // 确保字符串结束
    
    // 显示连接中的消息框
    show_message_box("Connecting...", "Attempting to connect to WiFi.");
    lv_timer_handler();  // 处理LVGL事件，确保消息框显示

    // WiFi连接流程
    Serial.println("断开现有WiFi连接...");
    WiFi.disconnect();  // 断开当前连接
    delay(100);  // 等待断开完成
    
    Serial.println("开始新的WiFi连接...");
    WiFi.begin(ssid, password);  // 开始连接到指定的WiFi网络
    int attempts = 0;  // 连接尝试计数器
    
    // 等待连接完成，最多尝试30次（15秒）
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);  // 每次等待500ms
        attempts++;  // 增加尝试次数
        Serial.print("连接尝试 "); Serial.print(attempts); Serial.print("/30, 状态: ");
        
        // 输出详细的连接状态信息，便于调试
        switch(WiFi.status()) {
            case WL_IDLE_STATUS: Serial.println("空闲"); break;  // WiFi模块空闲状态
            case WL_NO_SSID_AVAIL: Serial.println("找不到SSID"); break;  // 指定的SSID不存在
            case WL_SCAN_COMPLETED: Serial.println("扫描完成"); break;  // 网络扫描完成
            case WL_CONNECTED: Serial.println("已连接"); break;  // 成功连接到WiFi
            case WL_CONNECT_FAILED: Serial.println("连接失败"); break;  // 连接失败（通常是密码错误）
            case WL_CONNECTION_LOST: Serial.println("连接丢失"); break;  // 连接中断
            case WL_DISCONNECTED: Serial.println("已断开"); break;  // 已断开连接
            default: Serial.print("状态码: "); Serial.println(WiFi.status()); break;  // 其他未知状态
        }
        
        // 检查是否出现明确的错误状态，如果是则提前退出循环
        if (WiFi.status() == WL_NO_SSID_AVAIL || WiFi.status() == WL_CONNECT_FAILED) {
            Serial.println("检测到连接错误，提前退出");
            break;  // 跳出等待循环
        }
    }

    // 处理WiFi连接结果
    if (WiFi.status() == WL_CONNECTED) {
        // 连接成功的处理流程
        Serial.println("=== WiFi连接成功! ===");
        Serial.print("IP地址: "); Serial.println(WiFi.localIP());  // 输出获得的IP地址
        
        // 显示成功消息并保存设置
        show_message_box("Success", "WiFi connected successfully!\nSettings saved.");
        save_settings();  // 保存WiFi凭据到EEPROM
        updateStatus("WiFi Connected", lv_color_hex(0x10b981));  // 更新状态为绿色"已连接"
        fetchGitHubData();  // 立即获取GitHub数据
    } else {
        // 连接失败的处理流程
        Serial.println("=== WiFi连接失败! ===");
        Serial.print("最终状态: ");
        // 根据具体的失败原因显示不同的错误消息
        switch(WiFi.status()) {
            case WL_NO_SSID_AVAIL:
                Serial.println("找不到指定的SSID");
                show_message_box("Failure", "Network not found.\nPlease check SSID.");  // SSID不存在
                break;
            case WL_CONNECT_FAILED:
                Serial.println("连接失败，可能是密码错误");
                show_message_box("Failure", "Wrong password.\nPlease try again.");  // 密码错误
                break;
            default:
                Serial.print("其他错误，状态码: "); Serial.println(WiFi.status());
                show_message_box("Failure", "WiFi connection failed.\nPlease check settings.");  // 其他未知错误
                break;
        }
        updateStatus("WiFi Connection Failed!", lv_color_hex(0xef4444));  // 更新状态为红色"连接失败"
    }
    
    // 连接完成后返回设置界面
    lv_scr_load_anim(screen_settings, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, false);  // 使用右滑动画切换到设置屏幕
    control_buttons_visibility(screen_settings);  // 更新按钮显示状态
}

/**
 * 文本框事件处理函数
 * 功能：处理文本框的焦点和点击事件，控制虚拟键盘的显示
 * 参数：e - LVGL事件对象
 * 特点：
 * - 当文本框被点击或获得焦点时显示键盘
 * - 将键盘与当前文本框关联
 * - 失去焦点时不自动隐藏键盘（用户手动控制）
 */
static void ta_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);  // 获取事件类型
    lv_obj_t * ta = (lv_obj_t*)lv_event_get_target(e);  // 获取触发事件的文本框对象
    
    if(code == LV_EVENT_FOCUSED || code == LV_EVENT_CLICKED) {  // 文本框被点击或获得焦点
        Serial.println("文本框被点击/聚焦，显示键盘");
        if(kb != NULL) {  // 确保键盘对象存在
            lv_keyboard_set_textarea(kb, ta);  // 将键盘与当前文本框关联
            lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);  // 显示键盘
            lv_obj_move_foreground(kb);  // 将键盘移到前景
        }
    }
    else if(code == LV_EVENT_DEFOCUSED) {  // 文本框失去焦点
        Serial.println("文本框失去焦点，隐藏键盘");
        if(kb != NULL) {  // 确保键盘对象存在
            lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);  // 自动隐藏键盘
        }
    }
}

// ===== 天气数据管理函数实现 =====

/**
 * 获取天气数据函数
 * 功能：通过高德天气API获取当前位置的天气信息
 * 特点：使用高德天气API，需要API密钥，支持中国境内天气数据
 */
void fetchWeatherData() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi未连接，无法获取天气数据");
        Serial.printf("WiFi状态: %d\n", WiFi.status());
        return;
    }
    
    Serial.printf("WiFi连接状态: 已连接到 %s\n", WiFi.SSID().c_str());
    Serial.printf("本地IP地址: %s\n", WiFi.localIP().toString().c_str());
    
    // 检查是否需要更新天气数据
    unsigned long currentTime = millis();
    unsigned long timeSinceLastUpdate = currentTime - lastWeatherUpdate;
    
    Serial.printf("[DEBUG] 天气数据检查: 当前时间=%lu, 上次更新=%lu, 时间差=%lu, 更新间隔=%lu\n", 
                  currentTime, lastWeatherUpdate, timeSinceLastUpdate, WEATHER_UPDATE_INTERVAL);
    Serial.printf("[DEBUG] 天气数据有效性: %s\n", isWeatherDataValid ? "有效" : "无效");
    
    if (isWeatherDataValid && (timeSinceLastUpdate < WEATHER_UPDATE_INTERVAL)) {
        Serial.printf("天气数据仍然有效，跳过更新 (剩余时间: %lu毫秒)\n", WEATHER_UPDATE_INTERVAL - timeSinceLastUpdate);
        Serial.printf("[DEBUG] 当前天气数据: 温度=%.1f°C, 湿度=%d%%, 风力等级=%s, 天气=%s\n",
                  currentTemperature, currentHumidity, currentWindPower.c_str(), currentWeatherCondition.c_str());
        return;
    }
    
    Serial.println("开始获取天气数据...");
    Serial.println("当前位置ID: " + userLocation);
    
    WiFiClientSecure client;
    HTTPClient http;
    
    // 配置HTTPS客户端
    client.setInsecure(); // 跳过证书验证（生产环境建议使用证书）
    
    // 构建高德天气API请求URL
    String url = "https://" + String(WEATHER_API_HOST) + "/v3/weather/weatherInfo?city=" + userLocation + "&key=" + String(WEATHER_API_KEY);
    
    Serial.println("请求URL: " + url);
    Serial.println("开始发送HTTPS请求...");
    
    http.begin(client, url);
    http.setTimeout(15000); // 15秒超时
    http.addHeader("User-Agent", "ESP32-WeatherDisplay/1.0");
    http.addHeader("Accept", "application/json");
    http.addHeader("Accept-Encoding", "identity"); // 要求未压缩响应，避免解压缩复杂性
    http.addHeader("Connection", "close");
    
    int httpResponseCode = http.GET();
    
    if (httpResponseCode == 200) {
        // 直接获取未压缩的响应数据
        String payload = http.getString();
        Serial.println("接收到天气API响应");
        
        Serial.println("天气API响应: " + payload);
        
        // 解析高德天气API JSON响应
        DynamicJsonDocument doc(2048);
        DeserializationError error = deserializeJson(doc, payload);
        
        if (error) {
            Serial.println("JSON解析失败: " + String(error.c_str()));
            http.end();
            return;
        }
        
        // 检查API响应状态
        String status = doc["status"].as<String>();
        if (status != "1") {
            String info = doc["info"].as<String>();
            Serial.println("高德天气API错误，状态: " + status + ", 信息: " + info);
            http.end();
            return;
        }
        
        // 提取高德天气数据
        if (doc["lives"].is<JsonArray>() && doc["lives"].size() > 0) {
            JsonObject weather = doc["lives"][0];
            
            // 提取温度（摄氏度）
            currentTemperature = weather["temperature"].as<float>();
            
            // 提取湿度（百分比）
            currentHumidity = weather["humidity"].as<int>();
            
            // 提取风力等级（直接使用API返回的windpower值，并处理特殊符号）
            String rawWindPower = weather["windpower"].as<String>();
            currentWindPower = processWindPowerDisplay(rawWindPower);
            
            // 提取天气描述
            currentWeatherCondition = weather["weather"].as<String>();
            
            // 更新时间戳和状态
            lastWeatherUpdate = currentTime;
            isWeatherDataValid = true;
            
            Serial.printf("[DEBUG] 天气数据更新成功:\n");
            Serial.printf("[DEBUG] - 温度: %.1f°C\n", currentTemperature);
            Serial.printf("[DEBUG] - 湿度: %d%%\n", currentHumidity);
            Serial.printf("[DEBUG] - 风力等级: %s\n", currentWindPower.c_str());
            Serial.printf("[DEBUG] - 天气状况: %s\n", currentWeatherCondition.c_str());
            Serial.printf("[DEBUG] - 位置ID: %s\n", userLocation.c_str());
            Serial.printf("[DEBUG] - 更新时间戳: %lu\n", lastWeatherUpdate);
            
            // 更新天气显示
            Serial.println("[DEBUG] 开始更新天气显示界面...");
            updateWeatherDisplay();
            Serial.println("[DEBUG] 天气显示界面更新完成");
        } else {
            Serial.println("高德天气数据格式错误");
        }
    } else {
        Serial.printf("天气API请求失败，HTTP代码: %d\n", httpResponseCode);
        if (httpResponseCode > 0) {
            String errorResponse = http.getString();
            Serial.println("错误响应: " + errorResponse);
        } else {
            Serial.println("网络连接错误或超时");
        }
        
        // 标记天气数据为无效
        isWeatherDataValid = false;
        Serial.println("天气数据已标记为无效");
    }
    
    http.end();
}

/**
 * 从JSON文件获取城市名称函数
 * 功能：根据输入的位置ID，从city_adcodes.json文件中查找对应的城市名称
 * 参数：locationId - 位置ID（adcode）
 * 返回：对应的城市名称，如果未找到则返回"Location + ID"
 */
String getLocationName(const String& locationId) {
    // 使用头文件中的查找函数
    const CityData* cityData = findCityByAdcode(locationId.c_str());
    if (cityData != nullptr) {
        Serial.printf("找到位置ID %s 对应的城市: %s\n", locationId.c_str(), cityData->english);
        return String(cityData->english);
    }
    
    // 如果没有找到匹配的城市，返回默认格式
    Serial.printf("未找到位置ID %s 对应的城市\n", locationId.c_str());
    return "Location " + locationId;
}

/**
 * 处理风力等级显示字符串
 * 功能：将API返回的风力等级字符串中的特殊符号替换为更适合显示的表达
 * 参数：windPower - 原始风力等级字符串
 * 返回：处理后的风力等级字符串
 */
String processWindPowerDisplay(const String& windPower) {
    String processed = windPower;
    
    // 替换≤符号为更适合显示的表达
    processed.replace("≤", "<=");
    
    // 替换≥符号为更适合显示的表达
    processed.replace("≥", ">=");
    
    // 替换其他可能的特殊符号
    processed.replace("＜", "<");
    processed.replace("＞", ">");
    processed.replace("～", "-");
    
    return processed;
}

// 图表动画相关变量
static lv_coord_t* chart_target_data = NULL;  // 目标数据数组
static lv_coord_t* chart_current_data = NULL; // 当前动画数据数组
static int chart_anim_data_count = 0;  // 动画数据点数量
static bool chart_animation_running = false;  // 动画运行状态

/**
 * 图表数据点动画回调函数
 * 功能：实现数据点从底部平滑上升到目标值的动画效果
 * 参数：var - 动画变量（数据点索引），val - 动画进度值(0-100)
 */
static void chart_data_anim_cb(void* var, int32_t val) {
    if (!chart_animation_running || chart_target_data == NULL || chart_current_data == NULL) {
        return;
    }
    
    int point_index = (int)(intptr_t)var;
    
    // 计算当前数据点的动画值
    if (point_index < chart_anim_data_count) {
        // 使用缓出曲线，让动画更自然
        float progress = val / 100.0f;
        progress = 1.0f - (1.0f - progress) * (1.0f - progress);  // 缓出效果
        
        chart_current_data[point_index] = (lv_coord_t)(chart_target_data[point_index] * progress);
        
        // 更新图表显示
        updateChartDisplayWithAnimation();
    }
}

/**
 * 图表整体动画完成回调函数
 * 功能：动画完成后的清理工作
 */
static void chart_anim_ready_cb(lv_anim_t* a) {
    chart_animation_running = false;
    Serial.println("图表动画完成");
    
    // 确保最终显示正确的数据
    if (chart_target_data != NULL && chart_current_data != NULL) {
        for (int i = 0; i < chart_anim_data_count; i++) {
            chart_current_data[i] = chart_target_data[i];
        }
        updateChartDisplayWithAnimation();
    }
}

/**
 * 启动图表数据动画（无参数版本）
 * 功能：使用全局chart_data数组启动动画
 */
void startChartAnimation() {
    startChartAnimation(chart_data, chart_data_count);
}

/**
 * 启动图表数据动画
 * 功能：为图表数据点创建平滑的上升动画效果
 * 参数：target_data - 目标数据数组，data_count - 数据点数量
 */
void startChartAnimation(lv_coord_t* target_data, int data_count) {
    if (chart_obj == NULL || chart_series == NULL || target_data == NULL || data_count <= 0) {
        Serial.println("图表动画启动失败：参数无效");
        return;
    }
    
    Serial.println("启动图表数据动画");
    
    // 停止现有动画
    if (chart_animation_running) {
        lv_anim_del_all();
    }
    
    // 分配内存
    if (chart_target_data != NULL) {
        free(chart_target_data);
    }
    if (chart_current_data != NULL) {
        free(chart_current_data);
    }
    
    chart_target_data = (lv_coord_t*)malloc(data_count * sizeof(lv_coord_t));
    chart_current_data = (lv_coord_t*)malloc(data_count * sizeof(lv_coord_t));
    
    if (chart_target_data == NULL || chart_current_data == NULL) {
        Serial.println("图表动画内存分配失败");
        return;
    }
    
    // 复制目标数据并初始化当前数据为0
    for (int i = 0; i < data_count; i++) {
        chart_target_data[i] = target_data[i];
        chart_current_data[i] = 0;
    }
    
    chart_anim_data_count = data_count;
    chart_animation_running = true;
    
    // 根据数据点数量动态调整延迟时间（最多10个数据点）
    int base_delay = (data_count > 7) ? 30 : 50;  // 数据点较多时减少延迟
    int anim_duration = 300;  // 固定动画时长
    
    // 为每个数据点创建动画，添加延迟让数据点依次出现
    for (int i = 0; i < data_count; i++) {
        lv_anim_t anim;
        lv_anim_init(&anim);
        lv_anim_set_var(&anim, (void*)(intptr_t)i);  // 传递数据点索引
        lv_anim_set_values(&anim, 0, 100);  // 0-100的进度值
        lv_anim_set_time(&anim, anim_duration);  // 动态调整动画时长
        lv_anim_set_delay(&anim, i * base_delay);  // 动态调整延迟时间
        lv_anim_set_exec_cb(&anim, chart_data_anim_cb);
        lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);  // 缓出动画曲线
        
        // 只为最后一个数据点设置完成回调
        if (i == data_count - 1) {
            lv_anim_set_ready_cb(&anim, chart_anim_ready_cb);
        }
        
        lv_anim_start(&anim);
    }
}

/**
 * 带动画的图表显示更新函数
 * 功能：在动画过程中更新图表显示，不包含标签创建
 */
void updateChartDisplayWithAnimation() {
    if (chart_obj == NULL || chart_series == NULL || chart_current_data == NULL) {
        return;
    }
    
    // 清除现有数据
    lv_chart_set_point_count(chart_obj, chart_anim_data_count);
    
    // 设置当前动画数据
    for (int i = 0; i < chart_anim_data_count; i++) {
        lv_chart_set_next_value(chart_obj, chart_series, chart_current_data[i]);
    }
    
    // 自动调整Y轴范围（基于目标数据）
    if (chart_anim_data_count > 0 && chart_target_data != NULL) {
        lv_coord_t minVal = chart_target_data[0];
        lv_coord_t maxVal = chart_target_data[0];
        
        for (int i = 1; i < chart_anim_data_count; i++) {
            if (chart_target_data[i] < minVal) minVal = chart_target_data[i];
            if (chart_target_data[i] > maxVal) maxVal = chart_target_data[i];
        }
        
        // 添加一些边距
        lv_coord_t range = maxVal - minVal;
        lv_coord_t margin = range * 0.1;  // 10%边距
        if (margin < 1) margin = 1;
        
        lv_chart_set_range(chart_obj, LV_CHART_AXIS_PRIMARY_Y, minVal - margin, maxVal + margin);
    }
    
    lv_obj_invalidate(chart_obj);  // 刷新图表显示
}

/**
 * 定时器回调函数：延迟创建图表标签
 * 功能：用于定时器回调，创建图表标签后删除定时器
 */
static void chart_labels_timer_cb(lv_timer_t* timer) {
    updateChartLabels();
    lv_timer_del(timer);
}

/**
 * 更新图表标签显示
 * 功能：创建和更新图表的数据点标签和坐标轴标签
 */
void updateChartLabels() {
    if (chart_obj == NULL || chart_target_data == NULL || chart_anim_data_count <= 0) {
        return;
    }
    
    lv_obj_t* parent = lv_obj_get_parent(chart_obj);
    
    // 根据数据点数量动态调整标签延迟时间（最多10个数据点）
    int base_delay = (chart_anim_data_count > 7) ? 30 : 50;
    int label_base_offset = (chart_anim_data_count > 7) ? 150 : 200;
    int x_label_base_offset = (chart_anim_data_count > 7) ? 180 : 250;
    
    // 获取图表区域信息
    lv_area_t chart_area;
    lv_obj_get_coords(chart_obj, &chart_area);
    int chart_width = lv_area_get_width(&chart_area);
    int chart_height = lv_area_get_height(&chart_area);
    
    // 计算数据范围
    lv_coord_t minVal = chart_target_data[0];
    lv_coord_t maxVal = chart_target_data[0];
    
    for (int i = 1; i < chart_anim_data_count; i++) {
        if (chart_target_data[i] < minVal) minVal = chart_target_data[i];
        if (chart_target_data[i] > maxVal) maxVal = chart_target_data[i];
    }
    
    // 添加边距
    lv_coord_t range = maxVal - minVal;
    lv_coord_t margin = range * 0.1;
    if (margin < 1) margin = 1;
    
    // 定义容器边界变量
    lv_coord_t container_left_narrow = chart_area.x1 - 10;
    lv_coord_t container_right_narrow = chart_area.x2 + 10;
    lv_coord_t container_top = chart_area.y1 - 30;
    lv_coord_t container_left_wide = chart_area.x1 - 20;
    lv_coord_t container_right_wide = chart_area.x2 + 20;
    
    for (int i = 0; i < chart_anim_data_count; i++) {
        // 计算数据点在图表中的位置
        lv_coord_t x_pos;
        if (chart_anim_data_count == 1) {
            x_pos = chart_area.x1 + chart_width / 2;
        } else {
            x_pos = chart_area.x1 + (i * chart_width) / (chart_anim_data_count - 1);
        }
        lv_coord_t y_pos;
        if (maxVal - minVal + 2 * margin == 0) {
            y_pos = chart_area.y1 + chart_height / 2;
        } else {
            y_pos = chart_area.y2 - ((chart_target_data[i] - minVal + margin) * chart_height) / (maxVal - minVal + 2 * margin);
        }
        
        // 创建数据点数值标签
        lv_obj_t* value_label = lv_label_create(parent);
        lv_label_set_text_fmt(value_label, "%d", chart_target_data[i]);
        lv_obj_set_style_text_font(value_label, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(value_label, lv_color_hex(0xfbbf24), 0);
        
        // 添加标签出现动画
        lv_obj_set_style_opa(value_label, LV_OPA_TRANSP, 0);
        lv_anim_t label_anim;
        lv_anim_init(&label_anim);
        lv_anim_set_var(&label_anim, value_label);
        lv_anim_set_values(&label_anim, LV_OPA_TRANSP, LV_OPA_COVER);
        lv_anim_set_time(&label_anim, 300);
        lv_anim_set_delay(&label_anim, i * base_delay + label_base_offset);  // 在数据点动画后出现
        lv_anim_set_exec_cb(&label_anim, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
        lv_anim_set_path_cb(&label_anim, lv_anim_path_ease_in_out);
        lv_anim_start(&label_anim);
        
        // 获取标签尺寸以实现精确居中
        lv_obj_update_layout(value_label);
        lv_coord_t label_width = lv_obj_get_width(value_label);
        lv_coord_t label_height = lv_obj_get_height(value_label);
        
        // 检查是否所有数据相同
        bool all_same = true;
        for (int j = 1; j < chart_anim_data_count; j++) {
            if (chart_target_data[j] != chart_target_data[0]) {
                all_same = false;
                break;
            }
        }
        
        // 获取父容器坐标
        lv_area_t parent_area;
        lv_obj_get_coords(parent, &parent_area);
        
        // 计算标签位置
        lv_coord_t value_label_x = x_pos - label_width / 2;
        lv_coord_t value_label_y;
        
        if (all_same) {
            if (i % 2 == 0) {
                value_label_y = y_pos - label_height - 10;
            } else {
                value_label_y = y_pos + 1;
            }
        } else {
            value_label_y = y_pos - label_height - 2;
        }
        
        // 转换为相对坐标
        value_label_x -= parent_area.x1;
        value_label_y -= parent_area.y1;
        
        // 限制标签位置
        lv_coord_t container_left_rel = container_left_narrow - parent_area.x1;
        lv_coord_t container_right_rel = container_right_narrow - parent_area.x1;
        lv_coord_t container_top_rel = container_top - parent_area.y1;
        lv_coord_t container_bottom_rel = (chart_area.y2 + 30) - parent_area.y1;
        
        if (value_label_x < container_left_rel) value_label_x = container_left_rel;
        if (value_label_x + label_width > container_right_rel) value_label_x = container_right_rel - label_width;
        if (value_label_y < container_top_rel) value_label_y = container_top_rel;
        if (value_label_y + label_height > container_bottom_rel) value_label_y = container_bottom_rel - label_height;
        
        lv_obj_set_pos(value_label, value_label_x, value_label_y);
        lv_obj_set_user_data(value_label, (void*)0x1234);
        
        // 创建横坐标标签
        lv_obj_t* x_label = lv_label_create(parent);
        lv_obj_set_style_text_font(x_label, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(x_label, lv_color_hex(0x94a3b8), 0);
        lv_obj_set_user_data(x_label, (void*)0x5678);
        
        // 添加横坐标标签出现动画
        lv_obj_set_style_opa(x_label, LV_OPA_TRANSP, 0);
        lv_anim_t x_label_anim;
        lv_anim_init(&x_label_anim);
        lv_anim_set_var(&x_label_anim, x_label);
        lv_anim_set_values(&x_label_anim, LV_OPA_TRANSP, LV_OPA_COVER);
        lv_anim_set_time(&x_label_anim, 300);
        lv_anim_set_delay(&x_label_anim, i * base_delay + x_label_base_offset);  // 在数值标签后出现
        lv_anim_set_exec_cb(&x_label_anim, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
        lv_anim_set_path_cb(&x_label_anim, lv_anim_path_ease_in_out);
        lv_anim_start(&x_label_anim);
        
        // 根据图表模式设置横坐标标签文本
        if (chart_view_mode == 0) {
            if (i == chart_anim_data_count - 1) {
                lv_label_set_text(x_label, "now");
            } else {
                lv_label_set_text_fmt(x_label, "-%d", chart_anim_data_count - 1 - i);
            }
        } else if (chart_view_mode == 1) {
            const char* weekdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
            time_t now = time(NULL);
            struct tm* timeinfo = localtime(&now);
            int today_weekday = timeinfo->tm_wday;
            int day_offset = chart_anim_data_count - 1 - i;
            int weekday = (today_weekday - day_offset + 7) % 7;
            lv_label_set_text(x_label, weekdays[weekday]);
        } else if (chart_view_mode == 2) {
            lv_label_set_text_fmt(x_label, "G%d", i + 1);
        }
        
        // 获取横坐标标签尺寸
        lv_obj_update_layout(x_label);
        lv_coord_t x_label_width = lv_obj_get_width(x_label);
        lv_coord_t x_label_x = x_pos - x_label_width / 2;
        lv_coord_t x_label_y = chart_area.y2 - 2;
        
        // 转换为相对坐标
        x_label_x -= parent_area.x1;
        x_label_y -= parent_area.y1;
        
        // 限制标签位置
        lv_coord_t container_left_wide_rel = container_left_wide - parent_area.x1;
        lv_coord_t container_right_wide_rel = container_right_wide - parent_area.x1;
        
        if (x_label_x < container_left_wide_rel) x_label_x = container_left_wide_rel;
        if (x_label_x + x_label_width > container_right_wide_rel) x_label_x = container_right_wide_rel - x_label_width;
        
        lv_obj_set_pos(x_label, x_label_x, x_label_y);
    }
}

/**
 * 将中文天气状况转换为英文
 * 功能：将高德天气API返回的中文天气描述转换为对应的英文描述
 * 参数：chineseWeather - 中文天气描述
 * 返回：对应的英文天气描述
 */
String translateWeatherToEnglish(const String& chineseWeather) {
    if (chineseWeather == "晴") return "Sunny";
    if (chineseWeather == "多云") return "Cloudy";
    if (chineseWeather == "阴") return "Overcast";
    if (chineseWeather == "阵雨") return "Showers";
    if (chineseWeather == "雷阵雨") return "Thunderstorms";
    if (chineseWeather == "雷阵雨伴有冰雹") return "Thunderstorms with Hail";
    if (chineseWeather == "雨夹雪") return "Sleet";
    if (chineseWeather == "小雨") return "Light Rain";
    if (chineseWeather == "中雨") return "Moderate Rain";
    if (chineseWeather == "大雨") return "Heavy Rain";
    if (chineseWeather == "暴雨") return "Rainstorm";
    if (chineseWeather == "大暴雨") return "Heavy Rainstorm";
    if (chineseWeather == "特大暴雨") return "Severe Rainstorm";
    if (chineseWeather == "阵雪") return "Snow Showers";
    if (chineseWeather == "小雪") return "Light Snow";
    if (chineseWeather == "中雪") return "Moderate Snow";
    if (chineseWeather == "大雪") return "Heavy Snow";
    if (chineseWeather == "暴雪") return "Blizzard";
    if (chineseWeather == "雾") return "Fog";
    if (chineseWeather == "冻雨") return "Freezing Rain";
    if (chineseWeather == "沙尘暴") return "Sandstorm";
    if (chineseWeather == "小雨-中雨") return "Light to Moderate Rain";
    if (chineseWeather == "中雨-大雨") return "Moderate to Heavy Rain";
    if (chineseWeather == "大雨-暴雨") return "Heavy Rain to Rainstorm";
    if (chineseWeather == "暴雨-大暴雨") return "Rainstorm to Heavy Rainstorm";
    if (chineseWeather == "大暴雨-特大暴雨") return "Heavy to Severe Rainstorm";
    if (chineseWeather == "小雪-中雪") return "Light to Moderate Snow";
    if (chineseWeather == "中雪-大雪") return "Moderate to Heavy Snow";
    if (chineseWeather == "大雪-暴雪") return "Heavy Snow to Blizzard";
    if (chineseWeather == "浮尘") return "Dust";
    if (chineseWeather == "扬沙") return "Sand";
    if (chineseWeather == "强沙尘暴") return "Strong Sandstorm";
    if (chineseWeather == "霾") return "Haze";
    
    // 如果没有匹配的翻译，返回原文
    return chineseWeather;
}

/**
 * 根据城市名称获取位置ID
 * 功能：将用户输入的城市名称转换为高德地图API所需的位置ID
 * 参数：cityName - 城市名称
 * 返回：对应的位置ID，如果未找到则返回默认位置
 */
/**
 * 拼音到中文城市名称映射函数
 * 功能：将拼音输入转换为对应的中文城市名称
 * 参数：pinyin - 拼音输入
 * 返回：对应的中文城市名称，如果未找到则返回原输入
 */
/**
 * 根据拼音查找城市adcode函数（支持模糊搜索和缩写）
 * 功能：使用city_data.h中的模糊搜索功能查找城市信息
 * 参数：pinyin - 拼音城市名称或缩写
 * 返回：找到的城市adcode，未找到返回空字符串
 */
String getCityAdcodeByPinyin(const String& pinyin) {
    String lowerPinyin = pinyin;
    lowerPinyin.toLowerCase();
    
    // 使用头文件中的模糊搜索函数
    const CityData* cityData = findCityByFuzzySearch(lowerPinyin.c_str());
    if (cityData != nullptr) {
        Serial.printf("模糊搜索找到城市: %s -> %s (adcode: %s)\n", 
                     lowerPinyin.c_str(), cityData->english, cityData->adcode);
        return String(cityData->adcode);
    }
    
    Serial.printf("未找到城市: %s\n", lowerPinyin.c_str());
    return "";
}

/**
 * 获取城市位置ID函数
 * 功能：根据输入的城市名称，从city_data.h中查找对应的adcode
 * 参数：cityName - 城市名称（支持拼音输入）
 * 返回：对应的城市adcode，如果未找到则返回默认位置
 */
String getCityLocationId(const String& cityName) {
    String searchName = cityName;
    searchName.trim(); // 去除首尾空格
    searchName.toLowerCase(); // 转换为小写
    
    Serial.printf("查找城市: %s\n", searchName.c_str());
    
    // 首先尝试拼音查找
    String adcode = getCityAdcodeByPinyin(searchName);
    if (adcode != "") {
        return adcode;
    }
    
    // 如果拼音查找失败，返回默认位置
    Serial.printf("未找到城市 %s，使用默认位置\n", searchName.c_str());
    return DEFAULT_LOCATION;
}

/**
 * 更新天气显示函数
 * 功能：更新天气界面上的位置、温度、湿度、风力等级和天气状况显示
 */
void updateWeatherDisplay() {
    Serial.printf("[DEBUG] updateWeatherDisplay: screen_weather=%p, isWeatherDataValid=%s\n", 
                  screen_weather, isWeatherDataValid ? "true" : "false");
    
    if (screen_weather == NULL || !isWeatherDataValid) {
        Serial.println("[DEBUG] updateWeatherDisplay: 跳过更新 - 界面未创建或数据无效");
        return;
    }
    
    Serial.println("[DEBUG] updateWeatherDisplay: 开始更新各个显示元素...");
    
    // 更新天气显示元素的内容
    if (weather_location_label != NULL) {
        String locationName = getLocationName(userLocation);
        lv_label_set_text(weather_location_label, locationName.c_str());
        Serial.printf("[DEBUG] - 位置标签已更新: %s\n", locationName.c_str());
    } else {
        Serial.println("[DEBUG] - 位置标签为NULL，跳过更新");
    }
    
    if (weather_temp_label != NULL) {
        lv_label_set_text_fmt(weather_temp_label, "%.1f°C", currentTemperature);
        Serial.printf("[DEBUG] - 温度标签已更新: %.1f°C\n", currentTemperature);
    } else {
        Serial.println("[DEBUG] - 温度标签为NULL，跳过更新");
    }
    
    if (weather_condition_label != NULL) {
        String englishWeather = translateWeatherToEnglish(currentWeatherCondition);
        lv_label_set_text(weather_condition_label, englishWeather.c_str());
        Serial.printf("[DEBUG] - 天气状况标签已更新: %s -> %s\n", currentWeatherCondition.c_str(), englishWeather.c_str());
    } else {
        Serial.println("[DEBUG] - 天气状况标签为NULL，跳过更新");
    }
    
    if (weather_humidity_label != NULL) {
        lv_label_set_text_fmt(weather_humidity_label, "Humidity: %d%%", currentHumidity);
        Serial.printf("[DEBUG] - 湿度标签已更新: %d%%\n", currentHumidity);
    } else {
        Serial.println("[DEBUG] - 湿度标签为NULL，跳过更新");
    }
    
    if (weather_wind_label != NULL) {
        String displayWindPower = processWindPowerDisplay(currentWindPower);
        lv_label_set_text_fmt(weather_wind_label, "Wind: %s lvl", displayWindPower.c_str());
        Serial.printf("[DEBUG] - 风力等级标签已更新: %s (原始: %s)\n", displayWindPower.c_str(), currentWindPower.c_str());
    } else {
        Serial.println("[DEBUG] - 风力等级标签为NULL，跳过更新");
    }
    
    Serial.println("[DEBUG] updateWeatherDisplay: 所有天气显示元素更新完成");
}

/**
 * 加载天气设置函数
 * 功能：从NVS存储中加载用户设置的位置ID
 */
void loadWeatherSettings() {
    preferences.begin("weather", true); // 只读模式
    
    userLocation = preferences.getString("location", DEFAULT_LOCATION);
    
    preferences.end();
    
    Serial.println("天气设置已加载: 位置ID=" + userLocation);
}

/**
 * 保存天气设置函数
 * 功能：将用户设置的位置ID保存到NVS存储
 */
void saveWeatherSettings() {
    preferences.begin("weather", false); // 读写模式
    
    preferences.putString("location", userLocation);
    
    preferences.end();
    
    Serial.println("天气设置已保存: 位置ID=" + userLocation);
}

/**
 * 隐藏键盘事件处理函数
 * 功能：隐藏虚拟键盘
 * 参数：e - LVGL事件对象
 */
static void hide_keyboard_event_cb(lv_event_t * e) {
    if(kb != NULL) {  // 确保键盘对象存在
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);  // 隐藏键盘
        Serial.println("键盘已隐藏");
    }
}

/**
 * 显示键盘事件处理函数
 * 功能：显示虚拟键盘
 * 参数：e - LVGL事件对象
 */
static void show_keyboard_event_cb(lv_event_t * e) {
    if(kb != NULL) {  // 确保键盘对象存在
        lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);  // 显示键盘
        Serial.println("键盘已显示");
    }
}

/**
 * 保存城市事件处理函数
 * 功能：保存用户输入的城市名称，转换为位置ID并更新天气数据
 * 参数：e - LVGL事件对象
 */
/**
 * 城市保存按钮事件回调函数
 * 功能：保存用户输入的城市名称，转换为位置ID，保存设置并立即刷新天气数据
 * 参数：e - LVGL事件对象
 */
static void save_city_event_cb(lv_event_t * e) {
    lv_obj_t * btn = (lv_obj_t*)lv_event_get_target(e);  // 获取被点击的保存按钮
    lv_obj_t * ta = (lv_obj_t*)lv_event_get_user_data(e);  // 获取关联的文本框对象
    
    const char* cityName = lv_textarea_get_text(ta);  // 获取文本框中的城市名称
    Serial.printf("保存城市名称: %s\n", cityName);
    
    // 将城市名称转换为位置ID
    String newLocationId = getCityLocationId(cityName);
    Serial.printf("转换后的位置ID: %s\n", newLocationId.c_str());
    
    // 更新位置ID并保存设置（实现数据持久化）
    userLocation = newLocationId;
    saveWeatherSettings();
    
    // 立即刷新天气数据（重置更新时间戳以强制立即更新）
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("[DEBUG] 城市保存成功，准备立即刷新天气数据");
        Serial.printf("[DEBUG] 重置前 - lastWeatherUpdate: %lu, isWeatherDataValid: %s\n", 
                      lastWeatherUpdate, isWeatherDataValid ? "true" : "false");
        
        lastWeatherUpdate = 0;  // 重置天气更新时间戳，强制立即更新
        isWeatherDataValid = false;  // 标记当前数据为无效，确保强制更新
        
        Serial.printf("[DEBUG] 重置后 - lastWeatherUpdate: %lu, isWeatherDataValid: %s\n", 
                      lastWeatherUpdate, isWeatherDataValid ? "true" : "false");
        Serial.println("[DEBUG] 开始调用fetchWeatherData()...");
        
        fetchWeatherData();     // 立即获取新城市的天气数据
        
        Serial.println("[DEBUG] fetchWeatherData()调用完成");
    } else {
        Serial.printf("[DEBUG] WiFi未连接(状态: %d)，无法立即刷新天气数据\n", WiFi.status());
    }
    
    // 返回天气界面
    if (screen_weather != NULL) {
        Serial.println("[DEBUG] 切换回天气界面");
        lv_scr_load_anim(screen_weather, LV_SCR_LOAD_ANIM_MOVE_TOP, 200, 0, false);
        
        // 如果天气数据有效，立即更新显示
        Serial.printf("[DEBUG] 检查是否需要更新显示 - isWeatherDataValid: %s\n", 
                      isWeatherDataValid ? "true" : "false");
        if (isWeatherDataValid) {
            Serial.println("[DEBUG] 天气数据有效，立即更新显示");
            updateWeatherDisplay();
        } else {
            Serial.println("[DEBUG] 天气数据无效，跳过显示更新");
        }
    } else {
        Serial.println("[DEBUG] 天气界面为NULL，无法切换");
    }
}

// 全局变量声明，用于存储键盘对象
lv_obj_t* city_input_keyboard = NULL;

/**
 * 文本框事件回调函数
 * 功能：处理文本框的点击和失焦事件，控制键盘的显示和隐藏
 */
void city_input_ta_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * ta = (lv_obj_t*)lv_event_get_target(e);
    
    if(code == LV_EVENT_FOCUSED || code == LV_EVENT_CLICKED) {
        // 文本框被点击或获得焦点
        Serial.println("城市输入框被点击/聚焦，显示键盘");
        if(city_input_keyboard != NULL) {
            lv_keyboard_set_textarea(city_input_keyboard, ta);  // 将键盘与当前文本框关联
            lv_obj_clear_flag(city_input_keyboard, LV_OBJ_FLAG_HIDDEN);  // 显示键盘
            lv_obj_move_foreground(city_input_keyboard);  // 将键盘移到前景
        }
    }
    else if(code == LV_EVENT_DEFOCUSED) {
        // 文本框失去焦点
        Serial.println("城市输入框失去焦点，隐藏键盘");
        if(city_input_keyboard != NULL) {
            lv_obj_add_flag(city_input_keyboard, LV_OBJ_FLAG_HIDDEN);  // 自动隐藏键盘
        }
    }
}

/**
 * 创建城市输入界面
 * 功能：创建用于输入城市名称的界面，键盘按需显示
 */
void create_city_input_screen() {
    if(screen_city_input) lv_obj_del(screen_city_input);  // 如果界面已存在则先删除
    screen_city_input = lv_obj_create(NULL);  // 创建城市输入屏幕对象

    // 现代化背景设计
    lv_obj_set_style_bg_color(screen_city_input, lv_color_hex(0x1e293b), 0);  // 深蓝灰背景
    lv_obj_set_style_bg_grad_color(screen_city_input, lv_color_hex(0x0f172a), 0);  // 渐变
    lv_obj_set_style_bg_grad_dir(screen_city_input, LV_GRAD_DIR_VER, 0);

    // 创建现代化标题
    lv_obj_t* title = lv_label_create(screen_city_input);  // 创建标题标签
    lv_label_set_text(title, LV_SYMBOL_HOME " City Weather Settings");  // 使用LVGL内置家符号
    lv_obj_set_style_text_color(title, lv_color_hex(0xf1f5f9), 0);  // 浅色文字
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);  // 设置标题字体
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);  // 顶部居中对齐，向下偏移30像素

    // 创建现代化输入框
    lv_obj_t* ta = lv_textarea_create(screen_city_input);  // 创建文本输入区域
    lv_obj_set_size(ta, 280, 40);  // 设置输入框尺寸
    lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, 80);  // 向下偏移80像素
    lv_obj_set_style_bg_color(ta, lv_color_hex(0x334155), 0);  // 输入框背景色
    lv_obj_set_style_bg_opa(ta, LV_OPA_70, 0);  // 半透明背景
    lv_obj_set_style_border_color(ta, lv_color_hex(0x64748b), 0);  // 边框颜色
    lv_obj_set_style_border_width(ta, 1, 0);  // 细边框
    lv_obj_set_style_radius(ta, 8, 0);  // 圆角
    lv_obj_set_style_shadow_width(ta, 10, 0);  // 阴影宽度
    lv_obj_set_style_shadow_opa(ta, LV_OPA_30, 0);  // 阴影透明度
    
    // 设置提示文本
    lv_textarea_set_placeholder_text(ta, "Enter city name (Chinese/Pinyin)");
    lv_obj_set_style_text_color(ta, lv_color_hex(0xf1f5f9), 0);  // 文本颜色
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_14, 0);  // 文本字体
    lv_textarea_set_one_line(ta, true);  // 设置为单行输入
    
    // 添加文本框事件回调
    lv_obj_add_event_cb(ta, city_input_ta_event_cb, LV_EVENT_ALL, NULL);
    
    // 创建按钮容器（水平排列）
    lv_obj_t* btn_container = lv_obj_create(screen_city_input);
    lv_obj_set_size(btn_container, 260, 50);  // 设置容器尺寸
    lv_obj_align(btn_container, LV_ALIGN_TOP_MID, 0, 140);  // 向下偏移140像素
    lv_obj_set_style_bg_opa(btn_container, LV_OPA_TRANSP, 0);  // 透明背景
    lv_obj_set_style_border_width(btn_container, 0, 0);  // 无边框
    lv_obj_clear_flag(btn_container, LV_OBJ_FLAG_SCROLLABLE);  // 禁用滚动
    lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);  // 设置为水平布局
    lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);  // 均匀分布

    // 创建保存按钮
    lv_obj_t* save_btn = lv_btn_create(btn_container);  // 在容器中创建保存按钮
    lv_obj_set_size(save_btn, 120, 40);  // 设置按钮尺寸
    lv_obj_set_style_bg_color(save_btn, lv_color_hex(0x0ea5e9), 0);  // 蓝色按钮
    lv_obj_set_style_bg_grad_color(save_btn, lv_color_hex(0x0284c7), 0);  // 渐变
    lv_obj_set_style_bg_grad_dir(save_btn, LV_GRAD_DIR_VER, 0);  // 垂直渐变
    lv_obj_set_style_radius(save_btn, 8, 0);  // 圆角
    lv_obj_set_style_shadow_width(save_btn, 10, 0);  // 阴影宽度
    lv_obj_set_style_shadow_opa(save_btn, LV_OPA_30, 0);  // 阴影透明度
    
    // 保存按钮标签
    lv_obj_t* save_label = lv_label_create(save_btn);  // 创建按钮标签
    lv_label_set_text(save_label, LV_SYMBOL_SAVE " Save");  // 使用LVGL内置保存符号
    lv_obj_center(save_label);  // 居中对齐
    
    // 添加保存按钮事件回调
    lv_obj_add_event_cb(save_btn, save_city_event_cb, LV_EVENT_CLICKED, ta);

    // 创建取消按钮
    lv_obj_t* cancel_btn = lv_btn_create(btn_container);  // 在容器中创建取消按钮
    lv_obj_set_size(cancel_btn, 120, 40);  // 设置按钮尺寸
    lv_obj_set_style_bg_color(cancel_btn, lv_color_hex(0x64748b), 0);  // 灰色按钮
    lv_obj_set_style_radius(cancel_btn, 8, 0);  // 圆角
    
    // 取消按钮标签
    lv_obj_t* cancel_label = lv_label_create(cancel_btn);  // 创建按钮标签
    lv_label_set_text(cancel_label, LV_SYMBOL_CLOSE " Cancel");  // 使用LVGL内置关闭符号
    lv_obj_center(cancel_label);  // 居中对齐
    
    // 添加取消按钮事件回调
    lv_obj_add_event_cb(cancel_btn, [](lv_event_t * e) {
        // 隐藏键盘
        if(city_input_keyboard) {
            lv_obj_add_flag(city_input_keyboard, LV_OBJ_FLAG_HIDDEN);
        }
        // 返回天气界面
        if (screen_weather != NULL) {
            lv_scr_load_anim(screen_weather, LV_SCR_LOAD_ANIM_MOVE_TOP, 200, 0, false);
        }
    }, LV_EVENT_CLICKED, NULL);
    
    // 添加提示标签
    lv_obj_t* hint_label = lv_label_create(screen_city_input);  // 创建提示标签
    lv_label_set_text(hint_label, "Supports all cities in China\n\nSmart search with abbreviations:\nbj->beijing, sh->shanghai, gz->guangzhou\nnc->nanchang, cd->chengdu, wh->wuhan\n\nFull pinyin also supported:\nbeijing, shanghai, nanjing, hangzhou");
    lv_obj_set_style_text_color(hint_label, lv_color_hex(0x94a3b8), 0);  // 浅灰色文字
    lv_obj_set_style_text_font(hint_label, &lv_font_montserrat_10, 0);  // 小字体
    lv_obj_set_style_text_align(hint_label, LV_TEXT_ALIGN_CENTER, 0);  // 文本居中对齐
    lv_obj_align(hint_label, LV_ALIGN_TOP_MID, 0, 200);  // 向下偏移200像素
    
    // 创建虚拟键盘（默认隐藏）
    city_input_keyboard = lv_keyboard_create(screen_city_input);  // 创建虚拟键盘对象
    lv_keyboard_set_textarea(city_input_keyboard, ta);  // 将键盘与文本输入框关联
    lv_obj_align(city_input_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);  // 底部居中对齐
    lv_obj_add_flag(city_input_keyboard, LV_OBJ_FLAG_HIDDEN);  // 默认隐藏键盘
    
    Serial.println("城市输入界面创建完成");
}

/**
 * 编辑字段事件处理函数
 * 功能：处理GitHub设置中各个字段的编辑按钮点击事件
 * 参数：e - LVGL事件对象，按钮的用户数据包含字段类型
 * 特点：
 * - 根据字段类型创建相应的编辑界面
 * - 支持编辑Repository Owner、Repository Name和GitHub Token
 */
static void edit_field_event_cb(lv_event_t * e) {
    lv_obj_t * btn = (lv_obj_t*)lv_event_get_target(e);  // 获取被点击的按钮
    int field_type = (int)lv_obj_get_user_data(btn);  // 获取字段类型（从按钮用户数据）
    
    Serial.printf("编辑字段类型: %d\n", field_type);
    
    // 根据字段类型创建相应的编辑界面
    switch(field_type) {
        case 0: // Repository Owner（仓库所有者）
            create_edit_owner_screen();  // 创建所有者编辑界面
            lv_scr_load_anim(screen_edit_owner, LV_SCR_LOAD_ANIM_MOVE_TOP, 100, 0, false);  // 使用向上滑动画切换到所有者编辑屏幕
            break;
        case 1: // Repository Name（仓库名称）
            create_edit_repo_screen();  // 创建仓库名编辑界面
            lv_scr_load_anim(screen_edit_repo, LV_SCR_LOAD_ANIM_MOVE_TOP, 100, 0, false);  // 使用向上滑动画切换到仓库名编辑屏幕
            break;
        case 2: // GitHub Token（GitHub访问令牌）
            create_edit_token_screen();  // 创建令牌编辑界面
            lv_scr_load_anim(screen_edit_token, LV_SCR_LOAD_ANIM_MOVE_TOP, 100, 0, false);  // 使用向上滑动画切换到令牌编辑屏幕
            break;
    }
}

/**
 * 保存字段事件处理函数
 * 功能：处理编辑界面中保存按钮的点击事件
 * 参数：e - LVGL事件对象，用户数据包含文本框对象
 * 特点：
 * - 获取文本框中的内容并保存到相应的全局变量
 * - 保存设置到EEPROM
 * - 返回GitHub设置界面
 */
static void save_field_event_cb(lv_event_t * e) {
    lv_obj_t * btn = (lv_obj_t*)lv_event_get_target(e);  // 获取被点击的保存按钮
    lv_obj_t * ta = (lv_obj_t*)lv_event_get_user_data(e);  // 获取关联的文本框对象
    int field_type = (int)lv_obj_get_user_data(btn);  // 获取字段类型（从按钮用户数据）
    
    const char* text = lv_textarea_get_text(ta);  // 获取文本框中的内容
    Serial.printf("保存字段类型 %d: %s\n", field_type, text);
    
    // 根据字段类型将内容保存到相应的全局变量
    switch(field_type) {
        case 0: // Repository Owner（仓库所有者）
            strncpy(repoOwner, text, sizeof(repoOwner) - 1);  // 复制仓库所有者名称
            repoOwner[sizeof(repoOwner) - 1] = '\0';  // 确保字符串结束
            break;
        case 1: // Repository Name（仓库名称）
            strncpy(repoName, text, sizeof(repoName) - 1);  // 复制仓库名称
            repoName[sizeof(repoName) - 1] = '\0';  // 确保字符串结束
            break;
        case 2: // GitHub Token（GitHub访问令牌）
            strncpy(githubToken, text, sizeof(githubToken) - 1);  // 复制GitHub令牌
            githubToken[sizeof(githubToken) - 1] = '\0';  // 确保字符串结束
            break;
    }
    
    save_settings();  // 将所有设置保存到EEPROM
    
    // 显示保存成功消息
    show_message_box("Success", "Settings have been saved.");
    
    // 如果修改的是仓库名称，需要更新主页面的标题显示
    if(field_type == 1) {
        lv_label_set_text(title_label, repoName);
    }
    
    // 返回GitHub设置页面并刷新显示
    create_github_settings_screen();  // 重新创建GitHub设置界面（刷新显示的值）
    lv_scr_load_anim(screen_github_settings, LV_SCR_LOAD_ANIM_MOVE_BOTTOM, 100, 0, false);  // 使用向下滑动画切换到GitHub设置屏幕
}

/**
 * 设置按钮初始化函数
 * 功能：创建并配置右上角的设置按钮
 * 特点：
 * - 位于屏幕右上角，使用齿轮图标
 * - 圆形按钮，灰色背景
 * - 点击后进入设置界面
 * - 使用Font Awesome图标字体
 */
void setup_settings_button() {
    settings_btn = lv_btn_create(lv_layer_top());  // 在顶层创建按钮
    lv_obj_set_size(settings_btn, 35, 35);  // 设置按钮尺寸为35x35像素
    lv_obj_align(settings_btn, LV_ALIGN_TOP_RIGHT, -5, 5);  // 对齐到右上角，留5像素边距
    lv_obj_add_event_cb(settings_btn, settings_button_event_cb, LV_EVENT_CLICKED, NULL);  // 绑定点击事件
    lv_obj_set_style_radius(settings_btn, LV_RADIUS_CIRCLE, 0);  // 设置为圆形
    lv_obj_set_style_bg_color(settings_btn, lv_color_hex(0x6c757d), 0);  // 设置灰色背景
    lv_obj_set_style_border_width(settings_btn, 0, 0);  // 移除边框

    lv_obj_t *label = lv_label_create(settings_btn);  // 创建按钮标签
    lv_label_set_text(label, FA_GEAR);  // 使用Font Awesome齿轮图标
    lv_obj_set_style_text_font(label, &font_awesome_16, 0);  // 设置图标字体
    lv_obj_center(label);  // 居中显示图标
    Serial.println("功能性设置按钮已创建。");
}

/**
 * 触摸测试功能（条件编译）
 * 仅在定义了ENABLE_TOUCH_TEST宏时编译
 */
#ifdef ENABLE_TOUCH_TEST
/**
 * 专用触摸测试按钮事件处理函数
 * 功能：处理左上角触摸测试按钮的点击事件，现在具有刷新功能
 * 特点：
 * - 点击后按钮从蓝色变绿色
 * - 显示5秒倒计时刷新提示
 * - 进度条从100%每秒递减20%
 * - 倒计时结束后执行数据刷新
 * - 刷新完成后按钮变回蓝色
 */
void dedicated_touch_test_event_cb(lv_event_t *e) {
    Serial.println("专用触摸测试按钮（左上角）已成功点击！开始手动刷新流程");
    
    // 如果已经在刷新过程中，忽略点击
    if (isManualRefreshing || isFetchingData) {
        Serial.println("正在刷新中，忽略重复点击");
        return;
    }
    
    // 开始手动刷新流程
    isManualRefreshing = true;
    refreshButtonGreen = true;
    manualRefreshStartTime = millis();
    
    // 按钮变绿色
    lv_obj_set_style_bg_color(touch_test_btn, lv_color_hex(0x28A745), 0);
    
    // 显示刷新提示
    updateStatus("Data will refresh in 5 seconds", lv_color_hex(0x3b82f6));
    
    // 进度条设置为100%
    lv_obj_clear_flag(progress_bar, LV_OBJ_FLAG_HIDDEN);
    lv_bar_set_value(progress_bar, 100, LV_ANIM_OFF);
    
    Serial.println("手动刷新倒计时开始，按钮已变绿，进度条设为100%");
}

/**
 * 触摸测试按钮初始化函数
 * 功能：创建并配置左上角的触摸测试按钮
 * 特点：
 * - 位于屏幕左上角，显示字母"T"
 * - 圆形按钮，蓝色背景
 * - 用于测试触摸屏功能是否正常
 */
void setup_touch_test() {
    touch_test_btn = lv_btn_create(lv_layer_top());
    lv_obj_set_size(touch_test_btn, 35, 35);
    lv_obj_align(touch_test_btn, LV_ALIGN_TOP_LEFT, 5, 5);
    lv_obj_add_event_cb(touch_test_btn, dedicated_touch_test_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_radius(touch_test_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(touch_test_btn, lv_color_hex(0x007BFF), 0);
    
    lv_obj_t *label = lv_label_create(touch_test_btn);
    lv_label_set_text(label, LV_SYMBOL_REFRESH);  // 使用LVGL内置刷新符号
    lv_obj_center(label);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);  // 设置字体
    Serial.println("触摸测试按钮已创建。");
}
#endif

/**
 * ===== UI界面创建函数集合 =====
 * 以下函数负责创建应用程序的各个界面
 */

/**
 * 创建设置主界面
 * 功能：创建包含WiFi设置、GitHub设置和退出选项的设置菜单
 * 特点：
 * - 深色背景主题
 * - 使用列表组件展示设置选项
 * - 每个选项都有相应的图标
 * - 支持点击事件处理
 */
void create_settings_screen() {
    screen_settings = lv_obj_create(NULL);  // 创建设置屏幕对象
    
    // 现代化渐变背景，与主界面保持一致
    lv_obj_set_style_bg_color(screen_settings, lv_color_hex(0x0a0a0a), 0);
    lv_obj_set_style_bg_grad_color(screen_settings, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_bg_grad_dir(screen_settings, LV_GRAD_DIR_VER, 0);
    
    // 创建现代化标题容器
    lv_obj_t* title_container = lv_obj_create(screen_settings);
    lv_obj_set_size(title_container, 280, 50);
    lv_obj_align(title_container, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_bg_color(title_container, lv_color_hex(0x1e3a8a), 0);
    lv_obj_set_style_bg_grad_color(title_container, lv_color_hex(0x3b82f6), 0);
    lv_obj_set_style_bg_grad_dir(title_container, LV_GRAD_DIR_HOR, 0);
    lv_obj_set_style_border_width(title_container, 0, 0);
    lv_obj_set_style_radius(title_container, 15, 0);
    lv_obj_clear_flag(title_container, LV_OBJ_FLAG_SCROLLABLE);
    
    // 创建标题
    lv_obj_t* title = lv_label_create(title_container);
    lv_label_set_text(title, LV_SYMBOL_SETTINGS " Settings");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xffffff), 0);
    lv_obj_center(title);
    
    // 创建现代化设置选项容器
    lv_obj_t* options_container = lv_obj_create(screen_settings);
    lv_obj_set_size(options_container, 280, 190);
    lv_obj_align(options_container, LV_ALIGN_CENTER, 0, 35);
    lv_obj_set_style_bg_color(options_container, lv_color_hex(0x1f2937), 0);
    lv_obj_set_style_border_width(options_container, 1, 0);
    lv_obj_set_style_border_color(options_container, lv_color_hex(0x374151), 0);
    lv_obj_set_style_radius(options_container, 15, 0);
    lv_obj_set_style_pad_all(options_container, 15, 0);
    lv_obj_clear_flag(options_container, LV_OBJ_FLAG_SCROLLABLE);

    // WiFi设置按钮
    lv_obj_t* wifi_btn = lv_btn_create(options_container);
    lv_obj_set_size(wifi_btn, 250, 40);
    lv_obj_align(wifi_btn, LV_ALIGN_TOP_MID, 0, 15);
    lv_obj_set_style_bg_color(wifi_btn, lv_color_hex(0x059669), 0);
    lv_obj_set_style_bg_grad_color(wifi_btn, lv_color_hex(0x10b981), 0);
    lv_obj_set_style_bg_grad_dir(wifi_btn, LV_GRAD_DIR_HOR, 0);
    lv_obj_set_style_radius(wifi_btn, 10, 0);
    lv_obj_set_style_border_width(wifi_btn, 0, 0);
    lv_obj_add_event_cb(wifi_btn, settings_list_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t* wifi_label = lv_label_create(wifi_btn);
    lv_label_set_text(wifi_label, LV_SYMBOL_WIFI "  WiFi Setup");
    lv_obj_set_style_text_color(wifi_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(wifi_label, &lv_font_montserrat_16, 0);
    lv_obj_center(wifi_label);

    // GitHub设置按钮
    lv_obj_t* github_btn = lv_btn_create(options_container);
    lv_obj_set_size(github_btn, 250, 40);
    lv_obj_align(github_btn, LV_ALIGN_TOP_MID, 0, 70);
    lv_obj_set_style_bg_color(github_btn, lv_color_hex(0x7c3aed), 0);
    lv_obj_set_style_bg_grad_color(github_btn, lv_color_hex(0x8b5cf6), 0);
    lv_obj_set_style_bg_grad_dir(github_btn, LV_GRAD_DIR_HOR, 0);
    lv_obj_set_style_radius(github_btn, 10, 0);
    lv_obj_set_style_border_width(github_btn, 0, 0);
    lv_obj_add_event_cb(github_btn, settings_list_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t* github_label = lv_label_create(github_btn);
    lv_label_set_text(github_label, LV_SYMBOL_EDIT "  GitHub Setup");
    lv_obj_set_style_text_color(github_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(github_label, &lv_font_montserrat_16, 0);
    lv_obj_center(github_label);
    
    // 退出按钮
    lv_obj_t* exit_btn = lv_btn_create(options_container);
    lv_obj_set_size(exit_btn, 250, 40);
    lv_obj_align(exit_btn, LV_ALIGN_TOP_MID, 0, 125);
    lv_obj_set_style_bg_color(exit_btn, lv_color_hex(0xdc2626), 0);
    lv_obj_set_style_bg_grad_color(exit_btn, lv_color_hex(0xef4444), 0);
    lv_obj_set_style_bg_grad_dir(exit_btn, LV_GRAD_DIR_HOR, 0);
    lv_obj_set_style_radius(exit_btn, 10, 0);
    lv_obj_set_style_border_width(exit_btn, 0, 0);
    lv_obj_add_event_cb(exit_btn, settings_list_event_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t* exit_label = lv_label_create(exit_btn);
    lv_label_set_text(exit_label, LV_SYMBOL_CLOSE "  Exit");
    lv_obj_set_style_text_color(exit_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(exit_label, &lv_font_montserrat_16, 0);
    lv_obj_center(exit_label);
}

/**
 * 创建WiFi网络列表界面
 * 功能：显示可用的WiFi网络列表供用户选择
 * 特点：
 * - 显示扫描到的WiFi网络
 * - 支持点击选择网络
 * - 包含扫描状态提示
 * - 提供返回按钮
 */
void create_wifi_list_screen() {
    screen_wifi_list = lv_obj_create(NULL);  // 创建WiFi列表屏幕对象
    
    // 现代化背景设计
    lv_obj_set_style_bg_color(screen_wifi_list, lv_color_hex(0x0f172a), 0);  // 深蓝灰背景
    lv_obj_set_style_bg_grad_color(screen_wifi_list, lv_color_hex(0x1e293b), 0);  // 渐变色
    lv_obj_set_style_bg_grad_dir(screen_wifi_list, LV_GRAD_DIR_VER, 0);  // 垂直渐变
    
    // 创建现代化标题
    lv_obj_t* title = lv_label_create(screen_wifi_list);  // 创建标题标签
    lv_label_set_text(title, LV_SYMBOL_WIFI " WiFi Networks");  // 使用LVGL内置WiFi符号
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);  // 设置标题字体
    lv_obj_set_style_text_color(title, lv_color_hex(0xf8fafc), 0);  // 浅色文字
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);  // 顶部居中对齐，留15像素边距

    // 创建现代化WiFi网络列表
    lv_obj_t* list = lv_list_create(screen_wifi_list);  // 创建列表组件
    lv_obj_set_size(list, 300, 180);  // 设置列表尺寸（增加高度）
    lv_obj_align(list, LV_ALIGN_CENTER, 0, 10);  // 居中对齐，向下偏移10像素
    
    // 现代化列表样式
    lv_obj_set_style_bg_color(list, lv_color_hex(0x1e293b), 0);  // 深蓝灰背景，与屏幕背景协调
    lv_obj_set_style_bg_grad_color(list, lv_color_hex(0x334155), 0);  // 渐变
    lv_obj_set_style_bg_grad_dir(list, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_radius(list, 15, 0);  // 圆角
    lv_obj_set_style_border_width(list, 2, 0);  // 边框
    lv_obj_set_style_border_color(list, lv_color_hex(0x64748b), 0);
    lv_obj_set_style_shadow_width(list, 10, 0);  // 阴影
    lv_obj_set_style_shadow_color(list, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(list, LV_OPA_40, 0);

    // 创建现代化扫描状态标签
    lv_obj_t* label = lv_label_create(screen_wifi_list);  // 创建状态标签
    lv_label_set_text(label, LV_SYMBOL_REFRESH " Scanning for networks...");  // 使用LVGL内置刷新符号
    lv_obj_set_style_text_color(label, lv_color_hex(0x94a3b8), 0);  // 灰色文字
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);  // 设置字体
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);  // 居中显示

    // 创建现代化返回按钮
    lv_obj_t* back_btn = lv_btn_create(screen_wifi_list);  // 创建返回按钮
    lv_obj_set_size(back_btn, 100, 45);  // 设置按钮尺寸（增加高度）
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_LEFT, 15, -15);  // 左下角对齐，留15像素边距
    lv_obj_add_event_cb(back_btn, back_button_event_cb, LV_EVENT_CLICKED, screen_settings);  // 绑定返回事件
    
    // 现代化返回按钮样式
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0xdc2626), 0);  // 红色背景
    lv_obj_set_style_bg_grad_color(back_btn, lv_color_hex(0xef4444), 0);  // 渐变
    lv_obj_set_style_bg_grad_dir(back_btn, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_radius(back_btn, 12, 0);  // 圆角
    lv_obj_set_style_border_width(back_btn, 1, 0);  // 边框
    lv_obj_set_style_border_color(back_btn, lv_color_hex(0xfca5a5), 0);
    lv_obj_set_style_shadow_width(back_btn, 8, 0);  // 阴影
    lv_obj_set_style_shadow_color(back_btn, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(back_btn, LV_OPA_40, 0);
    
    lv_obj_t* back_label = lv_label_create(back_btn);  // 创建返回按钮标签
    lv_label_set_text(back_label, LV_SYMBOL_LEFT " Back");  // 使用LVGL内置左箭头符号
    lv_obj_set_style_text_color(back_label, lv_color_hex(0xffffff), 0);  // 白色文字
    lv_obj_center(back_label);  // 居中对齐
}

/**
 * 创建WiFi密码输入界面
 * 功能：为选定的WiFi网络创建密码输入界面
 * 参数：ssid_name - 选定的WiFi网络名称
 * 特点：
 * - 显示选定的WiFi网络名称
 * - 提供密码输入框
 * - 包含连接和返回按钮
 * - 支持虚拟键盘输入
 */
void create_wifi_password_screen(const char *ssid_name) {
    if(screen_wifi_password) lv_obj_del(screen_wifi_password);  // 如果界面已存在则先删除
    screen_wifi_password = lv_obj_create(NULL);  // 创建WiFi密码输入屏幕对象

    // 现代化背景设计
    lv_obj_set_style_bg_color(screen_wifi_password, lv_color_hex(0x0f172a), 0);  // 深蓝灰背景
    lv_obj_set_style_bg_grad_color(screen_wifi_password, lv_color_hex(0x1e293b), 0);  // 渐变色
    lv_obj_set_style_bg_grad_dir(screen_wifi_password, LV_GRAD_DIR_VER, 0);  // 垂直渐变

    // 创建现代化标题标签
    lv_obj_t* title = lv_label_create(screen_wifi_password);  // 创建标题标签
    lv_label_set_text_fmt(title, LV_SYMBOL_WIFI " Connect to %s", ssid_name);  // 使用LVGL内置WiFi符号
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);  // 设置标题字体
    lv_obj_set_style_text_color(title, lv_color_hex(0xf8fafc), 0);  // 浅色文字
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);  // 顶部居中对齐，留15像素边距

    // 创建现代化密码输入框
    lv_obj_t* pwd_ta = lv_textarea_create(screen_wifi_password);  // 创建文本输入区域
    lv_textarea_set_one_line(pwd_ta, true);  // 设置为单行输入
    lv_textarea_set_password_mode(pwd_ta, true);  // 启用密码模式（输入字符显示为*）
    lv_obj_set_size(pwd_ta, 280, 50);  // 设置尺寸（增加高度）
    lv_obj_align(pwd_ta, LV_ALIGN_TOP_MID, 0, 55);  // 顶部居中对齐，向下偏移55像素
    lv_textarea_set_placeholder_text(pwd_ta, "Enter WiFi password");  // 设置占位符文本
    lv_obj_add_event_cb(pwd_ta, ta_event_cb, LV_EVENT_ALL, NULL);  // 绑定文本框事件（用于显示虚拟键盘）
    
    // 现代化输入框样式
    lv_obj_set_style_bg_color(pwd_ta, lv_color_hex(0x334155), 0);  // 深灰背景
    lv_obj_set_style_bg_grad_color(pwd_ta, lv_color_hex(0x475569), 0);  // 渐变
    lv_obj_set_style_bg_grad_dir(pwd_ta, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_radius(pwd_ta, 12, 0);  // 圆角
    lv_obj_set_style_border_width(pwd_ta, 2, 0);  // 边框
    lv_obj_set_style_border_color(pwd_ta, lv_color_hex(0x64748b), 0);
    lv_obj_set_style_text_color(pwd_ta, lv_color_hex(0xf1f5f9), 0);  // 浅色文字
    lv_obj_set_style_shadow_width(pwd_ta, 8, 0);  // 阴影
    lv_obj_set_style_shadow_color(pwd_ta, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(pwd_ta, LV_OPA_30, 0);

    // 创建现代化连接按钮
    lv_obj_t* connect_btn = lv_btn_create(screen_wifi_password);  // 创建连接按钮
    lv_obj_set_size(connect_btn, 120, 45);  // 设置按钮尺寸（增加尺寸）
    lv_obj_align(connect_btn, LV_ALIGN_TOP_MID, 65, 120);  // 顶部居中对齐，向右偏移65像素，向下偏移120像素
    lv_obj_add_event_cb(connect_btn, wifi_connect_event_cb, LV_EVENT_CLICKED, pwd_ta);  // 绑定连接事件，传递密码输入框作为用户数据
    
    // 现代化连接按钮样式
    lv_obj_set_style_bg_color(connect_btn, lv_color_hex(0x059669), 0);  // 绿色背景
    lv_obj_set_style_bg_grad_color(connect_btn, lv_color_hex(0x10b981), 0);  // 渐变
    lv_obj_set_style_bg_grad_dir(connect_btn, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_radius(connect_btn, 12, 0);  // 圆角
    lv_obj_set_style_border_width(connect_btn, 1, 0);  // 边框
    lv_obj_set_style_border_color(connect_btn, lv_color_hex(0x34d399), 0);
    lv_obj_set_style_shadow_width(connect_btn, 8, 0);  // 阴影
    lv_obj_set_style_shadow_color(connect_btn, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(connect_btn, LV_OPA_40, 0);
    
    lv_obj_t* connect_label = lv_label_create(connect_btn);  // 创建连接按钮标签
    lv_label_set_text(connect_label, LV_SYMBOL_OK " Connect");  // 使用LVGL内置确认符号
    lv_obj_set_style_text_color(connect_label, lv_color_white(), 0);  // 设置文本颜色为白色
    lv_obj_center(connect_label);  // 居中对齐

    // 创建现代化返回按钮
    lv_obj_t* back_btn = lv_btn_create(screen_wifi_password);  // 创建返回按钮
    lv_obj_set_size(back_btn, 120, 45);  // 设置按钮尺寸（增加尺寸）
    lv_obj_align(back_btn, LV_ALIGN_TOP_MID, -65, 120);  // 顶部居中对齐，向左偏移65像素，向下偏移120像素
    lv_obj_add_event_cb(back_btn, back_button_event_cb, LV_EVENT_CLICKED, screen_wifi_list);  // 绑定返回事件，返回WiFi列表界面
    
    // 现代化返回按钮样式
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0xdc2626), 0);  // 红色背景
    lv_obj_set_style_bg_grad_color(back_btn, lv_color_hex(0xef4444), 0);  // 渐变
    lv_obj_set_style_bg_grad_dir(back_btn, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_radius(back_btn, 12, 0);  // 圆角
    lv_obj_set_style_border_width(back_btn, 1, 0);  // 边框
    lv_obj_set_style_border_color(back_btn, lv_color_hex(0xfca5a5), 0);
    lv_obj_set_style_shadow_width(back_btn, 8, 0);  // 阴影
    lv_obj_set_style_shadow_color(back_btn, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(back_btn, LV_OPA_40, 0);
    
    lv_obj_t* back_label = lv_label_create(back_btn);  // 创建返回按钮标签
    lv_label_set_text(back_label, LV_SYMBOL_LEFT " Back");  // 使用LVGL内置左箭头符号
    lv_obj_set_style_text_color(back_label, lv_color_white(), 0);  // 设置文本颜色为白色
    lv_obj_center(back_label);  // 居中对齐
    
    // 创建虚拟键盘（默认隐藏）
    kb = lv_keyboard_create(screen_wifi_password);  // 创建虚拟键盘对象
    lv_keyboard_set_textarea(kb, pwd_ta);  // 将键盘与密码输入框关联
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);  // 底部居中对齐
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);  // 默认隐藏键盘
}

/**
 * 创建GitHub仓库设置界面
 * 功能：提供GitHub仓库配置选项，包括仓库所有者、仓库名称和访问令牌设置
 * 特点：
 * - 三个主要设置项：Repository Owner、Repository Name、GitHub Token
 * - 每个设置项都有标签和可点击的按钮
 * - 显示当前设置值或提示用户设置
 * - 包含返回按钮
 */
void create_github_settings_screen() {
    screen_github_settings = lv_obj_create(NULL);  // 创建GitHub设置屏幕对象
    
    // 现代化背景设计
    lv_obj_set_style_bg_color(screen_github_settings, lv_color_hex(0x0f172a), 0);  // 深蓝灰背景
    lv_obj_set_style_bg_grad_color(screen_github_settings, lv_color_hex(0x1e293b), 0);  // 渐变色
    lv_obj_set_style_bg_grad_dir(screen_github_settings, LV_GRAD_DIR_VER, 0);  // 垂直渐变

    // 创建现代化标题
    lv_obj_t* title = lv_label_create(screen_github_settings);  // 创建标题标签
    lv_label_set_text(title, LV_SYMBOL_SETTINGS " GitHub Settings");  // 使用LVGL内置设置符号
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);  // 设置标题字体
    lv_obj_set_style_text_color(title, lv_color_hex(0xf8fafc), 0);  // 浅色文字
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);  // 顶部居中对齐，向下偏移15像素

    // 创建Repository Owner设置项（现代化设计）
    lv_obj_t* owner_label = lv_label_create(screen_github_settings);  // 创建所有者标签
    lv_label_set_text(owner_label, LV_SYMBOL_EDIT " Repository Owner");  // 使用LVGL内置编辑符号
    lv_obj_set_style_text_font(owner_label, &lv_font_montserrat_12, 0);  // 设置标签字体
    lv_obj_set_style_text_color(owner_label, lv_color_hex(0x94a3b8), 0);  // 灰色标签
    lv_obj_align(owner_label, LV_ALIGN_TOP_LEFT, 20, 55);  // 左上角对齐，留20像素左边距，向下偏移55像素

    lv_obj_t* owner_btn = lv_btn_create(screen_github_settings);  // 创建所有者设置按钮
    lv_obj_set_size(owner_btn, 280, 45);  // 设置按钮尺寸（增加高度）
    lv_obj_align(owner_btn, LV_ALIGN_TOP_MID, 0, 75);  // 顶部居中对齐，向下偏移75像素
    lv_obj_set_user_data(owner_btn, (void*)0);  // 设置用户数据，标识为owner编辑（字段类型0）
    lv_obj_add_event_cb(owner_btn, edit_field_event_cb, LV_EVENT_CLICKED, NULL);  // 绑定编辑字段事件
    
    // 现代化按钮样式
    lv_obj_set_style_bg_color(owner_btn, lv_color_hex(0x334155), 0);  // 深灰背景
    lv_obj_set_style_bg_grad_color(owner_btn, lv_color_hex(0x475569), 0);  // 渐变
    lv_obj_set_style_bg_grad_dir(owner_btn, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_radius(owner_btn, 12, 0);  // 圆角
    lv_obj_set_style_border_width(owner_btn, 1, 0);  // 边框
    lv_obj_set_style_border_color(owner_btn, lv_color_hex(0x64748b), 0);
    lv_obj_set_style_shadow_width(owner_btn, 8, 0);  // 阴影
    lv_obj_set_style_shadow_color(owner_btn, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(owner_btn, LV_OPA_30, 0);
    
    lv_obj_t* owner_btn_label = lv_label_create(owner_btn);  // 创建所有者按钮标签
    lv_label_set_text_fmt(owner_btn_label, "%s", strlen(repoOwner) > 0 ? repoOwner : "Tap to set owner");  // 设置按钮文本，显示当前值或提示
    lv_obj_set_style_text_align(owner_btn_label, LV_TEXT_ALIGN_LEFT, 0);  // 设置文本左对齐
    lv_obj_set_style_text_color(owner_btn_label, lv_color_hex(0xf1f5f9), 0);  // 浅色文字
    lv_obj_align(owner_btn_label, LV_ALIGN_LEFT_MID, 15, 0);  // 左侧对齐，留边距

    // 创建Repository Name设置项（现代化设计）
    lv_obj_t* repo_label = lv_label_create(screen_github_settings);  // 创建仓库名称标签
    lv_label_set_text(repo_label, LV_SYMBOL_DIRECTORY " Repository Name");  // 使用LVGL内置文件夹符号
    lv_obj_set_style_text_font(repo_label, &lv_font_montserrat_12, 0);  // 设置标签字体
    lv_obj_set_style_text_color(repo_label, lv_color_hex(0x94a3b8), 0);  // 灰色标签
    lv_obj_align(repo_label, LV_ALIGN_TOP_LEFT, 20, 135);  // 左上角对齐，留20像素左边距，向下偏移135像素

    lv_obj_t* repo_btn = lv_btn_create(screen_github_settings);  // 创建仓库名称设置按钮
    lv_obj_set_size(repo_btn, 280, 45);  // 设置按钮尺寸（增加高度）
    lv_obj_align(repo_btn, LV_ALIGN_TOP_MID, 0, 155);  // 顶部居中对齐，向下偏移155像素
    lv_obj_set_user_data(repo_btn, (void*)1);  // 设置用户数据，标识为repo编辑（字段类型1）
    lv_obj_add_event_cb(repo_btn, edit_field_event_cb, LV_EVENT_CLICKED, NULL);  // 绑定编辑字段事件
    
    // 现代化按钮样式
    lv_obj_set_style_bg_color(repo_btn, lv_color_hex(0x334155), 0);  // 深灰背景
    lv_obj_set_style_bg_grad_color(repo_btn, lv_color_hex(0x475569), 0);  // 渐变
    lv_obj_set_style_bg_grad_dir(repo_btn, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_radius(repo_btn, 12, 0);  // 圆角
    lv_obj_set_style_border_width(repo_btn, 1, 0);  // 边框
    lv_obj_set_style_border_color(repo_btn, lv_color_hex(0x64748b), 0);
    lv_obj_set_style_shadow_width(repo_btn, 8, 0);  // 阴影
    lv_obj_set_style_shadow_color(repo_btn, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(repo_btn, LV_OPA_30, 0);
    
    lv_obj_t* repo_btn_label = lv_label_create(repo_btn);  // 创建仓库名称按钮标签
    lv_label_set_text_fmt(repo_btn_label, "%s", strlen(repoName) > 0 ? repoName : "Tap to set repository");  // 设置按钮文本，显示当前值或提示
    lv_obj_set_style_text_align(repo_btn_label, LV_TEXT_ALIGN_LEFT, 0);  // 设置文本左对齐
    lv_obj_set_style_text_color(repo_btn_label, lv_color_hex(0xf1f5f9), 0);  // 浅色文字
    lv_obj_align(repo_btn_label, LV_ALIGN_LEFT_MID, 15, 0);  // 左侧对齐，留边距

    // 创建GitHub Token设置项（现代化设计）
    lv_obj_t* token_label = lv_label_create(screen_github_settings);  // 创建令牌标签
    lv_label_set_text(token_label, LV_SYMBOL_SETTINGS " GitHub Token (Optional)");  // 使用LVGL内置设置符号
    lv_obj_set_style_text_font(token_label, &lv_font_montserrat_12, 0);  // 设置标签字体
    lv_obj_set_style_text_color(token_label, lv_color_hex(0x94a3b8), 0);  // 灰色标签
    lv_obj_align(token_label, LV_ALIGN_TOP_LEFT, 20, 215);  // 左上角对齐，留20像素左边距，向下偏移215像素

    lv_obj_t* token_btn = lv_btn_create(screen_github_settings);  // 创建令牌设置按钮
    lv_obj_set_size(token_btn, 280, 45);  // 设置按钮尺寸（增加高度）
    lv_obj_align(token_btn, LV_ALIGN_TOP_MID, 0, 235);  // 顶部居中对齐，向下偏移235像素
    lv_obj_set_user_data(token_btn, (void*)2);  // 设置用户数据，标识为token编辑（字段类型2）
    lv_obj_add_event_cb(token_btn, edit_field_event_cb, LV_EVENT_CLICKED, NULL);  // 绑定编辑字段事件
    
    // 现代化按钮样式
    lv_obj_set_style_bg_color(token_btn, lv_color_hex(0x334155), 0);  // 深灰背景
    lv_obj_set_style_bg_grad_color(token_btn, lv_color_hex(0x475569), 0);  // 渐变
    lv_obj_set_style_bg_grad_dir(token_btn, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_radius(token_btn, 12, 0);  // 圆角
    lv_obj_set_style_border_width(token_btn, 1, 0);  // 边框
    lv_obj_set_style_border_color(token_btn, lv_color_hex(0x64748b), 0);
    lv_obj_set_style_shadow_width(token_btn, 8, 0);  // 阴影
    lv_obj_set_style_shadow_color(token_btn, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(token_btn, LV_OPA_30, 0);
    
    lv_obj_t* token_btn_label = lv_label_create(token_btn);  // 创建令牌按钮标签
    lv_label_set_text_fmt(token_btn_label, "%s", strlen(githubToken) > 0 ? "Token configured" : "Tap to set token");  // 设置按钮文本，显示配置状态或提示
    lv_obj_set_style_text_align(token_btn_label, LV_TEXT_ALIGN_LEFT, 0);  // 设置文本左对齐
    lv_obj_set_style_text_color(token_btn_label, lv_color_hex(0xf1f5f9), 0);  // 浅色文字
    lv_obj_align(token_btn_label, LV_ALIGN_LEFT_MID, 15, 0);  // 左侧对齐，留边距

    // 创建现代化返回按钮（固定在左下角）
    lv_obj_t* back_btn = lv_btn_create(screen_github_settings);  // 创建返回按钮
    lv_obj_set_size(back_btn, 100, 45);  // 设置按钮尺寸
    lv_obj_align(back_btn, LV_ALIGN_BOTTOM_LEFT, 15, -15);  // 左下角对齐，留15像素边距
    lv_obj_add_flag(back_btn, LV_OBJ_FLAG_FLOATING);  // 设置为浮动，不受滚动影响
    lv_obj_add_event_cb(back_btn, back_button_event_cb, LV_EVENT_CLICKED, screen_settings);  // 绑定返回事件，返回设置界面
    
    // 现代化返回按钮样式
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0xdc2626), 0);  // 红色背景
    lv_obj_set_style_bg_grad_color(back_btn, lv_color_hex(0xef4444), 0);  // 渐变
    lv_obj_set_style_bg_grad_dir(back_btn, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_radius(back_btn, 12, 0);  // 圆角
    lv_obj_set_style_border_width(back_btn, 1, 0);  // 边框
    lv_obj_set_style_border_color(back_btn, lv_color_hex(0xfca5a5), 0);
    lv_obj_set_style_shadow_width(back_btn, 8, 0);  // 阴影
    lv_obj_set_style_shadow_color(back_btn, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(back_btn, LV_OPA_40, 0);
    
    lv_obj_t* back_label = lv_label_create(back_btn);  // 创建返回按钮标签
    lv_label_set_text(back_label, LV_SYMBOL_LEFT " Back");  // 使用LVGL内置左箭头符号
    lv_obj_set_style_text_color(back_label, lv_color_hex(0xffffff), 0);  // 白色文字
    lv_obj_center(back_label);  // 居中对齐
}

/**
 * 创建仓库所有者编辑界面
 * 功能：提供文本输入界面用于编辑GitHub仓库所有者名称
 * 特点：
 * - 黑色背景的编辑界面
 * - 预填充当前仓库所有者值
 * - 包含保存和返回按钮
 * - 支持虚拟键盘输入
 * - 按钮布局避免键盘遮挡
 */
void create_edit_owner_screen() {
    screen_edit_owner = lv_obj_create(NULL);  // 创建仓库所有者编辑屏幕对象
    
    // 现代化背景设计
    lv_obj_set_style_bg_color(screen_edit_owner, lv_color_hex(0x1e293b), 0);  // 深蓝灰背景
    lv_obj_set_style_bg_grad_color(screen_edit_owner, lv_color_hex(0x0f172a), 0);  // 渐变
    lv_obj_set_style_bg_grad_dir(screen_edit_owner, LV_GRAD_DIR_VER, 0);

    // 创建现代化标题
    lv_obj_t* title = lv_label_create(screen_edit_owner);  // 创建标题标签
    lv_label_set_text(title, LV_SYMBOL_EDIT " Edit Repository Owner");  // 使用LVGL内置编辑符号
    lv_obj_set_style_text_color(title, lv_color_hex(0xf1f5f9), 0);  // 浅色文字
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);  // 设置标题字体
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);  // 顶部居中对齐，向下偏移20像素

    // 创建现代化输入框
    lv_obj_t* ta = lv_textarea_create(screen_edit_owner);  // 创建文本输入区域
    lv_obj_set_size(ta, 280, 50);  // 设置输入框尺寸（增加高度）
    lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, 70);  // 顶部居中对齐，向下偏移70像素
    lv_textarea_set_text(ta, repoOwner);  // 设置输入框初始文本为当前仓库所有者
    lv_textarea_set_placeholder_text(ta, "Enter repository owner");  // 设置占位符文本
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_14, 0);  // 设置输入框字体
    
    // 现代化输入框样式
    lv_obj_set_style_bg_color(ta, lv_color_hex(0x334155), 0);  // 深灰背景
    lv_obj_set_style_bg_grad_color(ta, lv_color_hex(0x475569), 0);  // 渐变
    lv_obj_set_style_bg_grad_dir(ta, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_radius(ta, 12, 0);  // 圆角
    lv_obj_set_style_border_width(ta, 2, 0);  // 边框
    lv_obj_set_style_border_color(ta, lv_color_hex(0x64748b), 0);
    lv_obj_set_style_shadow_width(ta, 8, 0);  // 阴影
    lv_obj_set_style_shadow_color(ta, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(ta, LV_OPA_30, 0);
    lv_obj_set_style_text_color(ta, lv_color_hex(0xf1f5f9), 0);  // 浅色文字

    // 创建现代化保存按钮
    lv_obj_t* save_btn = lv_btn_create(screen_edit_owner);  // 创建保存按钮
    lv_obj_set_size(save_btn, 100, 45);  // 设置按钮尺寸（增加尺寸）
    lv_obj_align(save_btn, LV_ALIGN_TOP_MID, 55, 140);  // 顶部居中对齐，向右偏移55像素，向下偏移140像素
    lv_obj_set_user_data(save_btn, (void*)0);  // 设置用户数据，标识为owner字段（字段类型0）
    lv_obj_add_event_cb(save_btn, save_field_event_cb, LV_EVENT_CLICKED, ta);  // 绑定保存事件，传递输入框作为用户数据
    
    // 现代化保存按钮样式
    lv_obj_set_style_bg_color(save_btn, lv_color_hex(0x059669), 0);  // 绿色背景
    lv_obj_set_style_bg_grad_color(save_btn, lv_color_hex(0x10b981), 0);  // 渐变
    lv_obj_set_style_bg_grad_dir(save_btn, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_radius(save_btn, 12, 0);  // 圆角
    lv_obj_set_style_border_width(save_btn, 1, 0);  // 边框
    lv_obj_set_style_border_color(save_btn, lv_color_hex(0x34d399), 0);
    lv_obj_set_style_shadow_width(save_btn, 8, 0);  // 阴影
    lv_obj_set_style_shadow_color(save_btn, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(save_btn, LV_OPA_40, 0);
    
    lv_obj_t* save_label = lv_label_create(save_btn);  // 创建保存按钮标签
    lv_label_set_text(save_label, LV_SYMBOL_SAVE " Save");  // 使用LVGL内置保存符号
    lv_obj_set_style_text_color(save_label, lv_color_white(), 0);  // 设置文本颜色为白色
    lv_obj_center(save_label);  // 居中对齐

    // 创建现代化返回按钮
    lv_obj_t* back_btn = lv_btn_create(screen_edit_owner);  // 创建返回按钮
    lv_obj_set_size(back_btn, 100, 45);  // 设置按钮尺寸（增加尺寸）
    lv_obj_align(back_btn, LV_ALIGN_TOP_MID, -55, 140);  // 顶部居中对齐，向左偏移55像素，向下偏移140像素
    lv_obj_add_event_cb(back_btn, back_button_event_cb, LV_EVENT_CLICKED, screen_github_settings);  // 绑定返回事件，返回GitHub设置界面
    
    // 现代化返回按钮样式
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0xdc2626), 0);  // 红色背景
    lv_obj_set_style_bg_grad_color(back_btn, lv_color_hex(0xef4444), 0);  // 渐变
    lv_obj_set_style_bg_grad_dir(back_btn, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_radius(back_btn, 12, 0);  // 圆角
    lv_obj_set_style_border_width(back_btn, 1, 0);  // 边框
    lv_obj_set_style_border_color(back_btn, lv_color_hex(0xfca5a5), 0);
    lv_obj_set_style_shadow_width(back_btn, 8, 0);  // 阴影
    lv_obj_set_style_shadow_color(back_btn, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(back_btn, LV_OPA_40, 0);
    
    lv_obj_t* back_label = lv_label_create(back_btn);  // 创建返回按钮标签
    lv_label_set_text(back_label, LV_SYMBOL_LEFT " Back");  // 设置按钮文本，添加箭头
    lv_obj_set_style_text_color(back_label, lv_color_white(), 0);  // 设置文本颜色为白色
    lv_obj_center(back_label);  // 居中对齐
    
    // 移除键盘控制按钮 - 现在键盘会在文本框聚焦时自动显示，失焦时自动隐藏

    // 创建虚拟键盘（默认隐藏）
    kb = lv_keyboard_create(screen_edit_owner);  // 创建虚拟键盘对象
    lv_keyboard_set_textarea(kb, ta);  // 将键盘与输入框关联
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);  // 底部居中对齐
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);  // 默认隐藏键盘
    
    // 为文本框添加事件处理，实现自动显示/隐藏键盘
    lv_obj_add_event_cb(ta, ta_event_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(ta, ta_event_cb, LV_EVENT_DEFOCUSED, NULL);
    lv_obj_add_event_cb(ta, ta_event_cb, LV_EVENT_CLICKED, NULL);
}

/**
 * 创建仓库名称编辑界面
 * 功能：提供文本输入界面用于编辑GitHub仓库名称
 * 特点：
 * - 黑色背景的编辑界面
 * - 预填充当前仓库名称值
 * - 包含保存和返回按钮
 * - 支持虚拟键盘输入
 * - 按钮布局避免键盘遮挡
 */
void create_edit_repo_screen() {
    screen_edit_repo = lv_obj_create(NULL);  // 创建仓库名称编辑屏幕对象
    
    // 现代化背景设计
    lv_obj_set_style_bg_color(screen_edit_repo, lv_color_hex(0x1e293b), 0);  // 深蓝灰背景
    lv_obj_set_style_bg_grad_color(screen_edit_repo, lv_color_hex(0x0f172a), 0);  // 渐变
    lv_obj_set_style_bg_grad_dir(screen_edit_repo, LV_GRAD_DIR_VER, 0);

    // 创建现代化标题
    lv_obj_t* title = lv_label_create(screen_edit_repo);  // 创建标题标签
    lv_label_set_text(title, LV_SYMBOL_DIRECTORY " Edit Repository Name");  // 使用LVGL内置文件夹符号
    lv_obj_set_style_text_color(title, lv_color_hex(0xf1f5f9), 0);  // 浅色文字
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);  // 设置标题字体
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);  // 顶部居中对齐，向下偏移20像素

    // 创建现代化输入框
    lv_obj_t* ta = lv_textarea_create(screen_edit_repo);  // 创建文本输入区域
    lv_obj_set_size(ta, 280, 50);  // 设置输入框尺寸（增加高度）
    lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, 70);  // 顶部居中对齐，向下偏移70像素
    lv_textarea_set_text(ta, repoName);  // 设置输入框初始文本为当前仓库名称
    lv_textarea_set_placeholder_text(ta, "Enter repository name");
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_14, 0);
    
    // 现代化输入框样式
    lv_obj_set_style_bg_color(ta, lv_color_hex(0x334155), 0);  // 深灰背景
    lv_obj_set_style_bg_grad_color(ta, lv_color_hex(0x475569), 0);  // 渐变
    lv_obj_set_style_bg_grad_dir(ta, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_radius(ta, 12, 0);  // 圆角
    lv_obj_set_style_border_width(ta, 2, 0);  // 边框
    lv_obj_set_style_border_color(ta, lv_color_hex(0x64748b), 0);
    lv_obj_set_style_shadow_width(ta, 8, 0);  // 阴影
    lv_obj_set_style_shadow_color(ta, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(ta, LV_OPA_30, 0);
    lv_obj_set_style_text_color(ta, lv_color_hex(0xf1f5f9), 0);  // 浅色文字

    // 创建现代化保存按钮
    lv_obj_t* save_btn = lv_btn_create(screen_edit_repo);
    lv_obj_set_size(save_btn, 100, 45);  // 增加尺寸
    lv_obj_align(save_btn, LV_ALIGN_TOP_MID, 55, 140);
    lv_obj_set_user_data(save_btn, (void*)1); // 标识为repo字段
    lv_obj_add_event_cb(save_btn, save_field_event_cb, LV_EVENT_CLICKED, ta);
    
    // 现代化保存按钮样式
    lv_obj_set_style_bg_color(save_btn, lv_color_hex(0x059669), 0);  // 绿色背景
    lv_obj_set_style_bg_grad_color(save_btn, lv_color_hex(0x10b981), 0);  // 渐变
    lv_obj_set_style_bg_grad_dir(save_btn, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_radius(save_btn, 12, 0);  // 圆角
    lv_obj_set_style_border_width(save_btn, 1, 0);  // 边框
    lv_obj_set_style_border_color(save_btn, lv_color_hex(0x34d399), 0);
    lv_obj_set_style_shadow_width(save_btn, 8, 0);  // 阴影
    lv_obj_set_style_shadow_color(save_btn, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(save_btn, LV_OPA_40, 0);
    
    lv_obj_t* save_label = lv_label_create(save_btn);
    lv_label_set_text(save_label, LV_SYMBOL_SAVE " Save");  // 使用LVGL内置保存符号
    lv_obj_set_style_text_color(save_label, lv_color_white(), 0);
    lv_obj_center(save_label);  // 居中对齐

    // 创建现代化返回按钮
    lv_obj_t* back_btn = lv_btn_create(screen_edit_repo);
    lv_obj_set_size(back_btn, 100, 45);  // 增加尺寸
    lv_obj_align(back_btn, LV_ALIGN_TOP_MID, -55, 140);
    lv_obj_add_event_cb(back_btn, back_button_event_cb, LV_EVENT_CLICKED, screen_github_settings);
    
    // 现代化返回按钮样式
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0xdc2626), 0);  // 红色背景
    lv_obj_set_style_bg_grad_color(back_btn, lv_color_hex(0xef4444), 0);  // 渐变
    lv_obj_set_style_bg_grad_dir(back_btn, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_radius(back_btn, 12, 0);  // 圆角
    lv_obj_set_style_border_width(back_btn, 1, 0);  // 边框
    lv_obj_set_style_border_color(back_btn, lv_color_hex(0xfca5a5), 0);
    lv_obj_set_style_shadow_width(back_btn, 8, 0);  // 阴影
    lv_obj_set_style_shadow_color(back_btn, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(back_btn, LV_OPA_40, 0);
    
    lv_obj_t* back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT " Back");  // 使用LVGL内置左箭头符号
    lv_obj_set_style_text_color(back_label, lv_color_white(), 0);
    lv_obj_center(back_label);  // 居中对齐
    
    // 移除键盘控制按钮 - 现在键盘会在文本框聚焦时自动显示，失焦时自动隐藏

    // 创建虚拟键盘（默认隐藏）
    kb = lv_keyboard_create(screen_edit_repo);
    lv_keyboard_set_textarea(kb, ta);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);  // 默认隐藏键盘
    
    // 为文本框添加事件处理，实现自动显示/隐藏键盘
    lv_obj_add_event_cb(ta, ta_event_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(ta, ta_event_cb, LV_EVENT_DEFOCUSED, NULL);
    lv_obj_add_event_cb(ta, ta_event_cb, LV_EVENT_CLICKED, NULL);
}

void create_edit_token_screen() {
    screen_edit_token = lv_obj_create(NULL);
    
    // 现代化背景设计
    lv_obj_set_style_bg_color(screen_edit_token, lv_color_hex(0x1e293b), 0);  // 深蓝灰背景
    lv_obj_set_style_bg_grad_color(screen_edit_token, lv_color_hex(0x0f172a), 0);  // 渐变
    lv_obj_set_style_bg_grad_dir(screen_edit_token, LV_GRAD_DIR_VER, 0);

    // 创建现代化标题
    lv_obj_t* title = lv_label_create(screen_edit_token);
    lv_label_set_text(title, LV_SYMBOL_SETTINGS " Edit GitHub Token");  // 使用LVGL内置设置符号
    lv_obj_set_style_text_color(title, lv_color_hex(0xf1f5f9), 0);  // 浅色文字
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);  // 向下偏移20像素

    // 创建现代化输入框
    lv_obj_t* ta = lv_textarea_create(screen_edit_token);
    lv_obj_set_size(ta, 280, 50);  // 增加高度
    lv_obj_align(ta, LV_ALIGN_TOP_MID, 0, 70);  // 向下偏移70像素
    lv_textarea_set_text(ta, githubToken);
    lv_textarea_set_placeholder_text(ta, "Enter GitHub token (optional)");
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_14, 0);
    
    // 现代化输入框样式
    lv_obj_set_style_bg_color(ta, lv_color_hex(0x334155), 0);  // 深灰背景
    lv_obj_set_style_bg_grad_color(ta, lv_color_hex(0x475569), 0);  // 渐变
    lv_obj_set_style_bg_grad_dir(ta, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_radius(ta, 12, 0);  // 圆角
    lv_obj_set_style_border_width(ta, 2, 0);  // 边框
    lv_obj_set_style_border_color(ta, lv_color_hex(0x64748b), 0);
    lv_obj_set_style_shadow_width(ta, 8, 0);  // 阴影
    lv_obj_set_style_shadow_color(ta, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(ta, LV_OPA_30, 0);
    lv_obj_set_style_text_color(ta, lv_color_hex(0xf1f5f9), 0);  // 浅色文字

    // 创建现代化保存按钮
    lv_obj_t* save_btn = lv_btn_create(screen_edit_token);
    lv_obj_set_size(save_btn, 100, 45);  // 增加尺寸
    lv_obj_align(save_btn, LV_ALIGN_TOP_MID, 55, 140);
    lv_obj_set_user_data(save_btn, (void*)2); // 标识为token字段
    lv_obj_add_event_cb(save_btn, save_field_event_cb, LV_EVENT_CLICKED, ta);
    
    // 现代化保存按钮样式
    lv_obj_set_style_bg_color(save_btn, lv_color_hex(0x059669), 0);  // 绿色背景
    lv_obj_set_style_bg_grad_color(save_btn, lv_color_hex(0x10b981), 0);  // 渐变
    lv_obj_set_style_bg_grad_dir(save_btn, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_radius(save_btn, 12, 0);  // 圆角
    lv_obj_set_style_border_width(save_btn, 1, 0);  // 边框
    lv_obj_set_style_border_color(save_btn, lv_color_hex(0x34d399), 0);
    lv_obj_set_style_shadow_width(save_btn, 8, 0);  // 阴影
    lv_obj_set_style_shadow_color(save_btn, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(save_btn, LV_OPA_40, 0);
    
    lv_obj_t* save_label = lv_label_create(save_btn);
    lv_label_set_text(save_label, LV_SYMBOL_SAVE " Save");  // 使用LVGL内置保存符号
    lv_obj_set_style_text_color(save_label, lv_color_white(), 0);
    lv_obj_center(save_label);  // 居中对齐

    // 创建现代化返回按钮
    lv_obj_t* back_btn = lv_btn_create(screen_edit_token);
    lv_obj_set_size(back_btn, 100, 45);  // 增加尺寸
    lv_obj_align(back_btn, LV_ALIGN_TOP_MID, -55, 140);
    lv_obj_add_event_cb(back_btn, back_button_event_cb, LV_EVENT_CLICKED, screen_github_settings);
    
    // 现代化返回按钮样式
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0xdc2626), 0);  // 红色背景
    lv_obj_set_style_bg_grad_color(back_btn, lv_color_hex(0xef4444), 0);  // 渐变
    lv_obj_set_style_bg_grad_dir(back_btn, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_radius(back_btn, 12, 0);  // 圆角
    lv_obj_set_style_border_width(back_btn, 1, 0);  // 边框
    lv_obj_set_style_border_color(back_btn, lv_color_hex(0xfca5a5), 0);
    lv_obj_set_style_shadow_width(back_btn, 8, 0);  // 阴影
    lv_obj_set_style_shadow_color(back_btn, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_opa(back_btn, LV_OPA_40, 0);
    
    lv_obj_t* back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT " Back");  // 使用LVGL内置左箭头符号
    lv_obj_set_style_text_color(back_label, lv_color_white(), 0);
    lv_obj_center(back_label);  // 居中对齐
    
    // 移除键盘控制按钮 - 现在键盘会在文本框聚焦时自动显示，失焦时自动隐藏

    // 创建虚拟键盘（默认隐藏）
    kb = lv_keyboard_create(screen_edit_token);
    lv_keyboard_set_textarea(kb, ta);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);  // 默认隐藏键盘
    
    // 为文本框添加事件处理，实现自动显示/隐藏键盘
    lv_obj_add_event_cb(ta, ta_event_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(ta, ta_event_cb, LV_EVENT_DEFOCUSED, NULL);
    lv_obj_add_event_cb(ta, ta_event_cb, LV_EVENT_CLICKED, NULL);
}

/**
 * 主界面滑动手势事件回调函数
 * 功能：处理主界面的左右滑动手势，实现天气和日历界面的切换
 * 参数：e - LVGL事件对象
 */
static void main_screen_gesture_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_GESTURE) {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
        
        if (dir == LV_DIR_LEFT) {
            // 左滑显示日历界面
            Serial.println("主界面左滑，切换到日历界面");
            // 删除旧的日历界面（如果存在）
            if (screen_calendar != NULL) {
                lv_obj_del(screen_calendar);
                screen_calendar = NULL;
            }
            // 重新创建日历界面
            create_calendar_screen();
            lv_scr_load_anim(screen_calendar, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
            control_buttons_visibility(screen_calendar);
            // 更新页面指示器状态
            updatePageIndicator(2);
        }
        else if (dir == LV_DIR_RIGHT) {
            // 右滑显示天气界面
            Serial.println("主界面右滑，切换到天气界面");
            if (screen_weather == NULL) {
                create_weather_screen();
            }
            lv_scr_load_anim(screen_weather, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0, false);
            control_buttons_visibility(screen_weather);
            // 更新页面指示器状态
            updatePageIndicator(0);
        }
    }
}

void createUI() {
    main_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(main_screen, lv_color_hex(0x0a0a0a), 0);
    lv_obj_set_style_bg_grad_color(main_screen, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_bg_grad_dir(main_screen, LV_GRAD_DIR_VER, 0);
    
    // 添加滑动手势支持
    lv_obj_add_event_cb(main_screen, main_screen_gesture_event_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_clear_flag(main_screen, LV_OBJ_FLAG_GESTURE_BUBBLE);
    
    // --- 标题区域 ---
    title_label = lv_label_create(main_screen);
    lv_label_set_text(title_label, repoName);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_20, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 5);

    // --- 时间显示区域 ---
    current_time_label = lv_label_create(main_screen);
    lv_label_set_text(current_time_label, "--:--:--");
    lv_obj_set_style_text_color(current_time_label, lv_color_hex(0x9ca3af), 0);
    lv_obj_set_style_text_font(current_time_label, &lv_font_montserrat_16, 0);
    lv_obj_align(current_time_label, LV_ALIGN_TOP_MID, 0, 30);

    // --- 主要星星显示区域 ---
    lv_obj_t* stars_container = lv_obj_create(main_screen);
    lv_obj_set_size(stars_container, 280, 100);
    lv_obj_align(stars_container, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_bg_color(stars_container, lv_color_hex(0x1e3a8a), 0);
    lv_obj_set_style_bg_grad_color(stars_container, lv_color_hex(0x3b82f6), 0);
    lv_obj_set_style_border_width(stars_container, 0, 0);
    lv_obj_set_style_radius(stars_container, 20, 0);
    lv_obj_clear_flag(stars_container, LV_OBJ_FLAG_SCROLLABLE);
    
    // 添加点击事件处理
    lv_obj_add_flag(stars_container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(stars_container, stars_card_event_cb, LV_EVENT_LONG_PRESSED, NULL);
    
    // 添加按下效果样式
    lv_obj_set_style_bg_color(stars_container, lv_color_hex(0x1e40af), LV_STATE_PRESSED);
    lv_obj_set_style_bg_grad_color(stars_container, lv_color_hex(0x2563eb), LV_STATE_PRESSED);

    lv_obj_t* stars_icon = lv_label_create(stars_container);
    lv_label_set_text(stars_icon, FA_STAR);
    lv_obj_set_style_text_font(stars_icon, &font_awesome_16, 0);
    lv_obj_set_style_text_color(stars_icon, lv_color_hex(0xfbbf24), 0);
    lv_obj_align(stars_icon, LV_ALIGN_CENTER, -80, -5);
    
    stars_count_label = lv_label_create(stars_container);
    lv_label_set_text(stars_count_label, "---");
    lv_obj_set_style_text_color(stars_count_label, lv_color_hex(0xfbbf24), 0);
    lv_obj_set_style_text_font(stars_count_label, &lv_font_montserrat_48, 0);
    lv_obj_align(stars_count_label, LV_ALIGN_CENTER, 10, -5);

    // 星标变化量标签
    stars_delta_label = lv_label_create(stars_container);
    lv_label_set_text(stars_delta_label, "");
    lv_obj_set_style_text_color(stars_delta_label, lv_color_hex(0x9ca3af), 0);
    lv_obj_set_style_text_font(stars_delta_label, &lv_font_montserrat_12, 0);
    lv_obj_align(stars_delta_label, LV_ALIGN_BOTTOM_LEFT, 10, -5);

    lv_obj_t* stars_text_label = lv_label_create(stars_container);
    lv_label_set_text(stars_text_label, "STARS");
    lv_obj_set_style_text_color(stars_text_label, lv_color_hex(0xe5e7eb), 0);
    lv_obj_set_style_text_font(stars_text_label, &lv_font_montserrat_14, 0);
    lv_obj_align(stars_text_label, LV_ALIGN_CENTER, 0, 30);

    // --- 分支和关注者信息区域 ---
    lv_obj_t* info_container = lv_obj_create(main_screen);
    lv_obj_set_size(info_container, 280, 40);
    lv_obj_align_to(info_container, stars_container, LV_ALIGN_OUT_BOTTOM_MID, 0, 15);
    lv_obj_set_style_bg_color(info_container, lv_color_hex(0x374151), 0);
    lv_obj_set_style_border_width(info_container, 0, 0);
    lv_obj_set_style_radius(info_container, 10, 0);
    lv_obj_clear_flag(info_container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* forks_icon = lv_label_create(info_container);
    lv_label_set_text(forks_icon, FA_CODE_BRANCH);
    lv_obj_set_style_text_font(forks_icon, &font_awesome_16, 0);
    lv_obj_set_style_text_color(forks_icon, lv_color_hex(0x10b981), 0);
    lv_obj_align(forks_icon, LV_ALIGN_LEFT_MID, 5, 0);
    
    forks_label = lv_label_create(info_container);
    lv_label_set_text(forks_label, "Forks: --");
    lv_obj_set_style_text_color(forks_label, lv_color_hex(0x10b981), 0);
    lv_obj_set_style_text_font(forks_label, &lv_font_montserrat_14, 0);
    lv_obj_align(forks_label, LV_ALIGN_LEFT_MID, 25, 0);

    lv_obj_t* watchers_icon = lv_label_create(info_container);
    lv_label_set_text(watchers_icon, FA_EYE);
    lv_obj_set_style_text_font(watchers_icon, &font_awesome_16, 0);
    lv_obj_set_style_text_color(watchers_icon, lv_color_hex(0x8b5cf6), 0);
    lv_obj_align(watchers_icon, LV_ALIGN_RIGHT_MID, -100, 0);
    
    watchers_label = lv_label_create(info_container);
    lv_label_set_text(watchers_label, "Watchers: --");
    lv_obj_set_style_text_color(watchers_label, lv_color_hex(0x8b5cf6), 0);
    lv_obj_set_style_text_font(watchers_label, &lv_font_montserrat_14, 0);
    lv_obj_align(watchers_label, LV_ALIGN_RIGHT_MID, -5, 0);

    // --- 状态栏 ---
    lv_obj_t* status_bar = lv_obj_create(main_screen);
    lv_obj_set_size(status_bar, 320, 30);
    lv_obj_align(status_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(status_bar, lv_color_hex(0x111827), 0);
    lv_obj_set_style_border_width(status_bar, 0, 0);
    lv_obj_set_style_radius(status_bar, 0, 0);
    lv_obj_set_style_pad_hor(status_bar, 10, 0);
    lv_obj_clear_flag(status_bar, LV_OBJ_FLAG_SCROLLABLE);
    
    status_label = lv_label_create(status_bar);
    lv_label_set_text(status_label, "Status: Initializing...");
    lv_obj_set_style_text_color(status_label, lv_color_hex(0xf59e0b), 0);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_12, 0);
    lv_obj_align(status_label, LV_ALIGN_LEFT_MID, 0, 0);

    time_label = lv_label_create(status_bar);
    lv_label_set_text(time_label, "Last Upd: --:--");
    lv_obj_set_style_text_color(time_label, lv_color_hex(0x9ca3af), 0);
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_12, 0);
    lv_obj_align(time_label, LV_ALIGN_RIGHT_MID, 0, 0);

    // --- 进度条 ---
    progress_bar = lv_bar_create(main_screen);
    lv_obj_set_size(progress_bar, 280, 5);
    lv_obj_align_to(progress_bar, info_container, LV_ALIGN_OUT_BOTTOM_MID, 0, 8);
    lv_obj_set_style_bg_color(progress_bar, lv_color_hex(0x374151), 0);
    lv_obj_set_style_bg_color(progress_bar, lv_color_hex(0x10b981), LV_PART_INDICATOR);
    lv_obj_add_flag(progress_bar, LV_OBJ_FLAG_HIDDEN);
    
    // 创建页面指示器
    createPageIndicator(main_screen);
    
    // 设置主页为当前页面（索引1）
    updatePageIndicator(1);

    Serial.println("UI创建完成。");
}

/**
 * 创建天气信息界面
 * 功能：显示当前天气信息、温度、湿度等数据
 */
void create_weather_screen() {
    Serial.println("=== 创建天气界面 ===");
    
    // 创建天气屏幕
    screen_weather = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_weather, lv_color_hex(0x0f172a), 0);  // 深蓝色背景
    lv_obj_set_style_bg_grad_color(screen_weather, lv_color_hex(0x1e3a8a), 0);  // 渐变色
    lv_obj_set_style_bg_grad_dir(screen_weather, LV_GRAD_DIR_VER, 0);
    
    // 添加滑动手势支持（返回主界面）
    lv_obj_add_event_cb(screen_weather, [](lv_event_t * e) {
        lv_event_code_t code = lv_event_get_code(e);
        if (code == LV_EVENT_GESTURE) {
            lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
            if (dir == LV_DIR_LEFT) {
                Serial.println("天气界面左滑，返回主界面");
                if (main_screen != NULL) {
                    lv_scr_load_anim(main_screen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
                    control_buttons_visibility(main_screen);
                    // 更新页面指示器状态
                    updatePageIndicator(1);
                } else {
                    Serial.println("错误：main_screen为NULL");
                }
            }
        }
    }, LV_EVENT_GESTURE, NULL);
    lv_obj_clear_flag(screen_weather, LV_OBJ_FLAG_GESTURE_BUBBLE);
    
    // 获取当前时间用于显示日期
    struct tm timeinfo;
    bool time_valid = false;
    
    // 检查WiFi连接状态和时间同步
    if (WiFi.status() == WL_CONNECTED) {
        time_valid = getLocalTime(&timeinfo);
        if (!time_valid) {
            // 如果NTP时间获取失败，尝试重新配置时间
            Serial.println("天气界面：NTP时间获取失败，尝试重新同步");
            configTime(8 * 3600, 0, "pool.ntp.org", "time.nist.gov");
            delay(1000);
            time_valid = getLocalTime(&timeinfo);
        }
    }
    
    // 创建天气标题（移到最上面，删除图标）
    lv_obj_t* title_label = lv_label_create(screen_weather);
    lv_label_set_text(title_label, "Weather");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0xf1f5f9), 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 10);
    
    // 创建日期显示（在标题下方）
    if (time_valid) {
        lv_obj_t* date_label = lv_label_create(screen_weather);
        char date_str[20];
        strftime(date_str, sizeof(date_str), "%Y-%m-%d", &timeinfo);
        lv_label_set_text(date_label, date_str);
        lv_obj_set_style_text_font(date_label, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(date_label, lv_color_hex(0xfbbf24), 0);
        lv_obj_align(date_label, LV_ALIGN_TOP_MID, 0, 35);
    }
    
    // 创建星期显示（在日期下方）
    if (time_valid) {
        lv_obj_t* weekday_label = lv_label_create(screen_weather);
        const char* weekdays[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
        lv_label_set_text(weekday_label, weekdays[timeinfo.tm_wday]);
        lv_obj_set_style_text_font(weekday_label, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(weekday_label, lv_color_hex(0xe5e7eb), 0);
        lv_obj_align(weekday_label, LV_ALIGN_TOP_MID, 0, 55);
    }
    
    // 创建天气信息容器（恢复正常位置）
    lv_obj_t* weather_container = lv_obj_create(screen_weather);
    lv_obj_set_size(weather_container, 260, 160);
    lv_obj_align(weather_container, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_bg_color(weather_container, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_border_width(weather_container, 0, 0);
    lv_obj_set_style_radius(weather_container, 20, 0);
    lv_obj_clear_flag(weather_container, LV_OBJ_FLAG_SCROLLABLE);
    
    // 添加长按事件，用于切换城市
    lv_obj_add_event_cb(weather_container, [](lv_event_t * e) {
        lv_event_code_t code = lv_event_get_code(e);
        if (code == LV_EVENT_LONG_PRESSED) {
            Serial.println("天气容器被长按，打开城市选择界面");
            create_city_input_screen();
            lv_scr_load_anim(screen_city_input, LV_SCR_LOAD_ANIM_MOVE_BOTTOM, 200, 0, false);
        }
    }, LV_EVENT_LONG_PRESSED, NULL);
    
    // 位置显示（保存全局引用）
    weather_location_label = lv_label_create(weather_container);
    String locationName = getLocationName(userLocation);
    lv_label_set_text(weather_location_label, locationName.c_str());
    lv_obj_set_style_text_font(weather_location_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(weather_location_label, lv_color_hex(0x9ca3af), 0);
    lv_obj_align(weather_location_label, LV_ALIGN_CENTER, 0, -50);
    
    // 温度显示（保存全局引用）
    weather_temp_label = lv_label_create(weather_container);
    lv_label_set_text(weather_temp_label, isWeatherDataValid ? (String(currentTemperature, 1) + "°C").c_str() : "--°C");
    lv_obj_set_style_text_font(weather_temp_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(weather_temp_label, lv_color_hex(0xfbbf24), 0);
    lv_obj_align(weather_temp_label, LV_ALIGN_CENTER, 0, -10);
    
    // 天气状况（保存全局引用）- 使用英文显示
    weather_condition_label = lv_label_create(weather_container);
    if (isWeatherDataValid) {
        String englishWeather = translateWeatherToEnglish(currentWeatherCondition);
        lv_label_set_text(weather_condition_label, englishWeather.c_str());
    } else {
        lv_label_set_text(weather_condition_label, "Loading...");
    }
    lv_obj_set_style_text_font(weather_condition_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(weather_condition_label, lv_color_hex(0xe5e7eb), 0);
    lv_obj_align(weather_condition_label, LV_ALIGN_CENTER, 0, 20);
    
    // 湿度信息（保存全局引用）
    weather_humidity_label = lv_label_create(weather_container);
    lv_label_set_text(weather_humidity_label, isWeatherDataValid ? ("Humidity: " + String(currentHumidity, 0) + "%").c_str() : "Humidity: --%");
    lv_obj_set_style_text_font(weather_humidity_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(weather_humidity_label, lv_color_hex(0x9ca3af), 0);
    lv_obj_align(weather_humidity_label, LV_ALIGN_BOTTOM_LEFT, 5, -10);
    
    // 风力等级信息（保存全局引用）
    weather_wind_label = lv_label_create(weather_container);
    String displayWindPower = isWeatherDataValid ? processWindPowerDisplay(currentWindPower) : "--";
    lv_label_set_text(weather_wind_label, isWeatherDataValid ? ("Wind: " + displayWindPower + " lvl").c_str() : "Wind: --");
    lv_obj_set_style_text_font(weather_wind_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(weather_wind_label, lv_color_hex(0x9ca3af), 0);
    lv_obj_align(weather_wind_label, LV_ALIGN_BOTTOM_RIGHT, -5, -10);
    
    // 如果天气数据无效且WiFi已连接，尝试获取天气数据
    if (!isWeatherDataValid && WiFi.status() == WL_CONNECTED) {
        Serial.println("天气界面：开始获取天气数据");
        fetchWeatherData();
    }
    
    // 添加调试按钮（手动刷新天气数据）- 与主页刷新按钮样式一致，位置在左上角
    lv_obj_t* refresh_btn = lv_btn_create(screen_weather);
    lv_obj_set_size(refresh_btn, 40, 40);
    lv_obj_align(refresh_btn, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_set_style_radius(refresh_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(refresh_btn, lv_color_hex(0x007BFF), 0);
    
    lv_obj_t* refresh_label = lv_label_create(refresh_btn);
    lv_label_set_text(refresh_label, LV_SYMBOL_REFRESH);
    lv_obj_center(refresh_label);
    lv_obj_set_style_text_font(refresh_label, &lv_font_montserrat_16, 0);
    
    // 添加按钮点击事件
    lv_obj_add_event_cb(refresh_btn, [](lv_event_t * e) {
        lv_event_code_t code = lv_event_get_code(e);
        if (code == LV_EVENT_CLICKED) {
            Serial.println("手动刷新天气数据");
            if (WiFi.status() == WL_CONNECTED) {
                fetchWeatherData();
            } else {
                Serial.println("WiFi未连接，无法获取天气数据");
            }
        }
    }, LV_EVENT_CLICKED, NULL);
    

    
    // 创建页面指示器
    createPageIndicator(screen_weather);
    
    // 设置天气页为当前页面（索引0）
    updatePageIndicator(0);
    
    Serial.println("天气界面创建完成");
}

/**
 * 创建日历界面
 * 功能：显示当前日期、月历视图等信息
 */
void create_calendar_screen() {
    Serial.println("=== 创建日历界面 ===");
    
    // 创建日历屏幕
    screen_calendar = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_calendar, lv_color_hex(0x0a0a0a), 0);  // 与主页相同的背景色
    lv_obj_set_style_bg_grad_color(screen_calendar, lv_color_hex(0x1a1a2e), 0);  // 与主页相同的渐变色
    lv_obj_set_style_bg_grad_dir(screen_calendar, LV_GRAD_DIR_VER, 0);
    
    // 添加滑动手势支持（返回主界面）
    lv_obj_add_event_cb(screen_calendar, [](lv_event_t * e) {
        lv_event_code_t code = lv_event_get_code(e);
        if (code == LV_EVENT_GESTURE) {
            lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
            if (dir == LV_DIR_RIGHT) {
                Serial.println("日历界面右滑，返回主界面");
                if (main_screen != NULL) {
                    lv_scr_load_anim(main_screen, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0, false);
                    control_buttons_visibility(main_screen);
                    // 更新页面指示器状态
                    updatePageIndicator(1);
                } else {
                    Serial.println("错误：main_screen为NULL");
                }
            }
        }
    }, LV_EVENT_GESTURE, NULL);
    lv_obj_clear_flag(screen_calendar, LV_OBJ_FLAG_GESTURE_BUBBLE);
    
    // 获取当前真实时间（统一声明）
    struct tm timeinfo;
    bool time_valid = false;
    
    // 检查WiFi连接状态和时间同步
    if (WiFi.status() == WL_CONNECTED) {
        time_valid = getLocalTime(&timeinfo);
        if (time_valid) {
            // 输出详细的时间调试信息
            const char* debug_weekdays[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
            Serial.println("=== 日历界面时间调试信息 ===");
            Serial.printf("年份: %d, 月份: %d, 日期: %d\n", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
            Serial.printf("星期: %s (tm_wday=%d)\n", debug_weekdays[timeinfo.tm_wday], timeinfo.tm_wday);
            Serial.printf("时间: %02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
            Serial.println("================================");
        } else {
            // 如果NTP时间获取失败，尝试重新配置时间
            Serial.println("日历界面：NTP时间获取失败，尝试重新同步");
            configTime(8 * 3600, 0, "pool.ntp.org", "time.nist.gov");
            delay(1000);
            time_valid = getLocalTime(&timeinfo);
            if (time_valid) {
                const char* debug_weekdays[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
                Serial.println("=== 重新同步后的时间信息 ===");
                Serial.printf("年份: %d, 月份: %d, 日期: %d\n", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
                Serial.printf("星期: %s (tm_wday=%d)\n", debug_weekdays[timeinfo.tm_wday], timeinfo.tm_wday);
                Serial.printf("时间: %02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
                Serial.println("==============================");
            }
        }
    }
    
    // 创建日期显示（在屏幕顶部）
    if (time_valid) {
        lv_obj_t* date_label = lv_label_create(screen_calendar);
        char date_str[20];
        strftime(date_str, sizeof(date_str), "%Y-%m-%d", &timeinfo);
        lv_label_set_text(date_label, date_str);
        lv_obj_set_style_text_font(date_label, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(date_label, lv_color_hex(0xfbbf24), 0);
        lv_obj_align(date_label, LV_ALIGN_TOP_MID, 0, 10);
    }
    
    // 创建星期显示（在日期下方）
    if (time_valid) {
        lv_obj_t* weekday_label = lv_label_create(screen_calendar);
        const char* weekdays[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
        lv_label_set_text(weekday_label, weekdays[timeinfo.tm_wday]);
        lv_obj_set_style_text_font(weekday_label, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(weekday_label, lv_color_hex(0xe5e7eb), 0);
        lv_obj_align(weekday_label, LV_ALIGN_TOP_MID, 0, 35);
    }
    
    // 创建日历标题
    lv_obj_t* title_label = lv_label_create(screen_calendar);
    lv_label_set_text(title_label, "Calendar");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0xf1f5f9), 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 60);
    
    // 创建日历容器（保持原始大小，调整位置避免遮盖）
    lv_obj_t* calendar_container = lv_obj_create(screen_calendar);
    lv_obj_set_size(calendar_container, 280, 160);  // 保持合适大小
    lv_obj_align(calendar_container, LV_ALIGN_BOTTOM_MID, 0, -30);  // 向下调整位置
    lv_obj_set_style_bg_color(calendar_container, lv_color_hex(0x1a1a2e), 0);
    lv_obj_set_style_border_width(calendar_container, 0, 0);
    lv_obj_set_style_radius(calendar_container, 20, 0);
    lv_obj_clear_flag(calendar_container, LV_OBJ_FLAG_SCROLLABLE);
    
    // 星期标题行（调整间距）
    const char* weekdays[] = {"S", "M", "T", "W", "T", "F", "S"};
    for (int i = 0; i < 7; i++) {
        lv_obj_t* day_header = lv_label_create(calendar_container);
        lv_label_set_text(day_header, weekdays[i]);
        lv_obj_set_style_text_font(day_header, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(day_header, lv_color_hex(0x9ca3af), 0);
        lv_obj_align(day_header, LV_ALIGN_TOP_LEFT, 25 + i * 35, 12);  // 调整间距和位置
    }
    
    // 正确的日期网格（根据实际星期排列）
    if (time_valid) {
        int current_day = timeinfo.tm_mday;
        int current_month = timeinfo.tm_mon;
        int current_year = timeinfo.tm_year + 1900;
        
        // 计算当月第一天是星期几
        struct tm first_day = {0};
        first_day.tm_year = current_year - 1900;
        first_day.tm_mon = current_month;
        first_day.tm_mday = 1;
        mktime(&first_day);  // 计算星期几
        int first_weekday = first_day.tm_wday;  // 0=Sunday, 1=Monday, ..., 6=Saturday
        
        // 计算当月天数
        int days_in_month;
        if (current_month == 1) {  // 2月
            bool is_leap = (current_year % 4 == 0 && current_year % 100 != 0) || (current_year % 400 == 0);
            days_in_month = is_leap ? 29 : 28;
        } else if (current_month == 3 || current_month == 5 || current_month == 8 || current_month == 10) {  // 4,6,9,11月
            days_in_month = 30;
        } else {  // 1,3,5,7,8,10,12月
            days_in_month = 31;
        }
        
        Serial.printf("=== 日历布局调试信息 ===\n");
        Serial.printf("当前日期: %d年%d月%d日\n", current_year, current_month + 1, current_day);
        Serial.printf("当月第一天是星期: %d (0=Sunday)\n", first_weekday);
        Serial.printf("当月总天数: %d\n", days_in_month);
        Serial.printf("========================\n");
        
        // 计算上个月的天数
        int prev_month = current_month - 1;
        int prev_year = current_year;
        if (prev_month < 0) {
            prev_month = 11;
            prev_year--;
        }
        
        int prev_month_days;
        if (prev_month == 1) {  // 2月
            bool is_leap = (prev_year % 4 == 0 && prev_year % 100 != 0) || (prev_year % 400 == 0);
            prev_month_days = is_leap ? 29 : 28;
        } else if (prev_month == 3 || prev_month == 5 || prev_month == 8 || prev_month == 10) {  // 4,6,9,11月
            prev_month_days = 30;
        } else {  // 1,3,5,7,8,10,12月
            prev_month_days = 31;
        }
        
        // 计算需要显示的周数（智能显示，避免过多下个月日期）
        int total_cells_needed = first_weekday + days_in_month;
        int weeks_needed = (total_cells_needed + 6) / 7;  // 向上取整
        if (weeks_needed > 5) weeks_needed = 5;  // 最多显示5周
        
        // 绘制优化的日历网格（减少不必要的下个月日期）
        for (int week = 0; week < weeks_needed; week++) {
            for (int day = 0; day < 7; day++) {
                int date_num = week * 7 + day + 1 - first_weekday;
                
                // 跳过超出必要范围的下个月日期
                if (date_num > days_in_month && date_num > days_in_month + 2) {
                    continue;  // 只显示下个月的1-2号
                }
                
                lv_obj_t* day_label = lv_label_create(calendar_container);
                char day_text[4];
                
                if (date_num < 1) {
                    // 上个月的日期
                    int prev_date = prev_month_days + date_num;
                    snprintf(day_text, sizeof(day_text), "%d", prev_date);
                    lv_label_set_text(day_label, day_text);
                    lv_obj_set_style_text_font(day_label, &lv_font_montserrat_12, 0);
                    lv_obj_set_style_text_color(day_label, lv_color_hex(0x6b7280), 0);  // 灰色表示上个月
                } else if (date_num > days_in_month) {
                    // 下个月的日期（只显示1-2号）
                    int next_date = date_num - days_in_month;
                    if (next_date <= 2) {  // 只显示下个月的1-2号
                        snprintf(day_text, sizeof(day_text), "%d", next_date);
                        lv_label_set_text(day_label, day_text);
                        lv_obj_set_style_text_font(day_label, &lv_font_montserrat_12, 0);
                        lv_obj_set_style_text_color(day_label, lv_color_hex(0x6b7280), 0);  // 灰色表示下个月
                    } else {
                        // 不显示过多的下个月日期，删除这个标签
                        lv_obj_del(day_label);
                        continue;
                    }
                } else {
                    // 当前月的日期
                    snprintf(day_text, sizeof(day_text), "%d", date_num);
                    lv_label_set_text(day_label, day_text);
                    lv_obj_set_style_text_font(day_label, &lv_font_montserrat_12, 0);
                    
                    // 高亮当前日期
                    if (date_num == current_day) {
                        lv_obj_set_style_text_color(day_label, lv_color_hex(0xfbbf24), 0);  // 黄色高亮当天
                        // 添加背景高亮
                        lv_obj_set_style_bg_color(day_label, lv_color_hex(0x3b82f6), 0);
                        lv_obj_set_style_bg_opa(day_label, LV_OPA_30, 0);
                        lv_obj_set_style_radius(day_label, 8, 0);
                    } else {
                        lv_obj_set_style_text_color(day_label, lv_color_hex(0xe5e7eb), 0);  // 白色表示当前月其他日期
                    }
                }
                
                lv_obj_align(day_label, LV_ALIGN_TOP_LEFT, 25 + day * 35, 32 + week * 18);  // 调整行间距
            }
        }
    } else {
        // 如果时间无效，显示提示信息
        lv_obj_t* error_label = lv_label_create(calendar_container);
        lv_label_set_text(error_label, "Time sync required");
        lv_obj_set_style_text_font(error_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(error_label, lv_color_hex(0xef4444), 0);
        lv_obj_center(error_label);
    }
    
    // 创建页面指示器
    createPageIndicator(screen_calendar);
    
    // 设置日历页为当前页面（索引2）
    updatePageIndicator(2);
    
    Serial.println("日历界面创建完成");
}

// ===== 核心逻辑 =====
/**
 * 从NVS（非易失性存储）加载用户配置设置
 * 功能：系统启动时从Flash存储中读取用户之前保存的配置
 * 包括：WiFi凭据（SSID、密码）和GitHub设置（仓库所有者、仓库名、访问令牌）
 * 特点：使用ESP32的Preferences库，数据在断电后仍然保持
 */
void load_settings() {
    Serial.println("=== 加载设置开始 ===");
    // 以只读模式打开NVS命名空间"gh-display"
    preferences.begin("gh-display", true);
    // 从NVS读取各项配置到全局变量
    preferences.getString("ssid", ssid, sizeof(ssid));           // WiFi网络名称
    preferences.getString("password", password, sizeof(password)); // WiFi密码
    preferences.getString("repoOwner", repoOwner, sizeof(repoOwner)); // GitHub仓库所有者
    preferences.getString("repoName", repoName, sizeof(repoName));   // GitHub仓库名称
    preferences.getString("githubToken", githubToken, sizeof(githubToken)); // GitHub访问令牌
    yesterdayStars = preferences.getInt("yesterdayStars", -1);   // 昨日星标数量
    lastDailySaveTime = preferences.getULong("lastDailySave", 0); // 上次每日保存时间
    lastSavedStars = preferences.getInt("lastSavedStars", -1);   // 上次保存的星标数
    preferences.end();  // 关闭NVS访问
    
    Serial.println("加载的设置:");
    Serial.print("  SSID: "); Serial.println(ssid);
    Serial.print("  密码长度: "); Serial.println(strlen(password));
    Serial.print("  仓库所有者: "); Serial.println(repoOwner);
    Serial.print("  仓库名称: "); Serial.println(repoName);
    Serial.print("  GitHub Token长度: "); Serial.println(strlen(githubToken));
    Serial.println("=== 加载设置完成 ===");
}

/**
 * 将当前配置设置保存到NVS（非易失性存储）
 * 功能：用户修改配置后，将新设置永久保存到Flash存储
 * 包括：WiFi凭据（SSID、密码）和GitHub设置（仓库所有者、仓库名、访问令牌）
 * 特点：数据保存后即使断电重启也不会丢失
 */
void save_settings() {
    Serial.println("=== 保存设置开始 ===");
    // 以读写模式打开NVS命名空间"gh-display"
    preferences.begin("gh-display", false);
    // 将全局变量中的配置写入NVS
    preferences.putString("ssid", ssid);           // 保存WiFi网络名称
    preferences.putString("password", password);   // 保存WiFi密码
    preferences.putString("repoOwner", repoOwner); // 保存GitHub仓库所有者
    preferences.putString("repoName", repoName);   // 保存GitHub仓库名称
    preferences.putString("githubToken", githubToken); // 保存GitHub访问令牌
    preferences.putInt("yesterdayStars", yesterdayStars); // 保存昨日星标数量
    preferences.putULong("lastDailySave", lastDailySaveTime); // 保存上次每日保存时间
    preferences.putInt("lastSavedStars", lastSavedStars); // 保存上次保存的星标数
    preferences.end();  // 关闭NVS访问，确保数据写入Flash
    
    Serial.println("保存的设置:");
    Serial.print("  SSID: "); Serial.println(ssid);
    Serial.print("  密码长度: "); Serial.println(strlen(password));
    Serial.print("  仓库所有者: "); Serial.println(repoOwner);
    Serial.print("  仓库名称: "); Serial.println(repoName);
    Serial.print("  GitHub Token长度: "); Serial.println(strlen(githubToken));
    Serial.println("=== 保存设置完成 ===");
}

/**
 * WiFi网络连接函数
 * 功能：使用保存的WiFi凭据连接到指定网络
 * 返回值：true=连接成功，false=连接失败
 * 特点：
 * - 自动重试连接（最多30次，每次间隔500ms）
 * - 详细的连接状态监控和错误诊断
 * - 实时更新UI状态显示
 * - 连接成功后显示网络信息（IP、信号强度等）
 */
bool connectWiFi() {
    Serial.println("=== WiFi连接开始 ===");
    updateStatus("Connecting to WiFi...", lv_color_hex(0xf59e0b));  // 更新UI状态
    
    Serial.print("目标SSID: "); Serial.println(ssid);
    Serial.print("密码长度: "); Serial.println(strlen(password));
    
    // 断开现有连接，确保干净的连接状态
    WiFi.disconnect();
    delay(100);
    
    // 设置WiFi为Station模式（客户端模式）
    Serial.println("设置WiFi模式为STA...");
    WiFi.mode(WIFI_STA);
    delay(100);
    
    // 开始连接到指定的WiFi网络
    Serial.println("开始连接WiFi...");
    WiFi.begin(ssid, password);
    
    // 连接重试循环（最多30次尝试，总计15秒）
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);  // 每次尝试间隔500ms
        attempts++;
        Serial.print("连接尝试 "); Serial.print(attempts); 
        Serial.print("/30, 状态: ");
        
        // 详细的WiFi连接状态监控，便于问题诊断
        switch(WiFi.status()) {
            case WL_IDLE_STATUS:
                Serial.println("空闲状态");  // WiFi模块空闲
                break;
            case WL_NO_SSID_AVAIL:
                Serial.println("找不到SSID");  // 指定的网络名称不存在
                break;
            case WL_SCAN_COMPLETED:
                Serial.println("扫描完成");  // 网络扫描已完成
                break;
            case WL_CONNECTED:
                Serial.println("已连接");  // 连接成功
                break;
            case WL_CONNECT_FAILED:
                Serial.println("连接失败");  // 连接失败（通常是密码错误）
                break;
            case WL_CONNECTION_LOST:
                Serial.println("连接丢失");  // 连接中断
                break;
            case WL_DISCONNECTED:
                Serial.println("已断开");  // 主动断开连接
                break;
            default:
                Serial.print("未知状态: "); Serial.println(WiFi.status());
                break;
        }
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("=== WiFi连接成功! ===");
        Serial.print("IP地址: "); Serial.println(WiFi.localIP());
        Serial.print("信号强度: "); Serial.print(WiFi.RSSI()); Serial.println(" dBm");
        Serial.print("网关: "); Serial.println(WiFi.gatewayIP());
        Serial.print("DNS: "); Serial.println(WiFi.dnsIP());
        
        updateStatus("WiFi Connected", lv_color_hex(0x10b981));
        delay(1000);
        return true;
    } else {
        Serial.println("=== WiFi连接失败! ===");
        Serial.print("最终状态: ");
        switch(WiFi.status()) {
            case WL_NO_SSID_AVAIL:
                Serial.println("找不到指定的SSID，请检查网络名称");
                break;
            case WL_CONNECT_FAILED:
                Serial.println("连接失败，可能是密码错误");
                break;
            case WL_CONNECTION_LOST:
                Serial.println("连接丢失");
                break;
            case WL_DISCONNECTED:
                Serial.println("连接被断开");
                break;
            default:
                Serial.print("连接超时，状态码: "); Serial.println(WiFi.status());
                break;
        }
        Serial.print("尝试次数: "); Serial.print(attempts); Serial.println("/30");
        
        updateStatus("WiFi Connection Failed", lv_color_hex(0xef4444));
        return false;
    }
}

/**
 * 串口命令处理函数
 * 功能：监听串口输入，处理动画测试命令
 * 支持的命令：
 * - "test_anim" - 测试数字滚动动画效果
 */
void handleSerialCommands() {
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();  // 移除首尾空白字符
        
        if (command == "test_anim") {
            Serial.println("=== 开始测试数字滚动动画 ===");
            testNumberAnimation();
        } else if (command == "show_time") {
            Serial.println("=== 当前时间信息 ===");
            struct tm timeinfo;
            if (getLocalTime(&timeinfo)) {
                const char* weekdays[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
                Serial.printf("年份: %d\n", timeinfo.tm_year + 1900);
                Serial.printf("月份: %d\n", timeinfo.tm_mon + 1);
                Serial.printf("日期: %d\n", timeinfo.tm_mday);
                Serial.printf("星期: %s (tm_wday=%d)\n", weekdays[timeinfo.tm_wday], timeinfo.tm_wday);
                Serial.printf("时间: %02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
                Serial.printf("完整日期: %04d-%02d-%02d %s\n", 
                    timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, weekdays[timeinfo.tm_wday]);
            } else {
                Serial.println("无法获取时间信息，请检查NTP同步状态");
            }
        } else if (command.startsWith("set_timezone ")) {
            String timezoneStr = command.substring(12);
            int timezone = timezoneStr.toInt();
            Serial.printf("=== 设置时区为 UTC%+d ===\n", timezone);
            configTime(timezone * 3600, 0, "pool.ntp.org", "time.nist.gov");
            Serial.println("等待时间同步...");
            delay(2000);
            struct tm timeinfo;
            if (getLocalTime(&timeinfo)) {
                const char* weekdays[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
                Serial.printf("时区设置成功！当前时间: %04d-%02d-%02d %s %02d:%02d:%02d\n", 
                    timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, 
                    weekdays[timeinfo.tm_wday], timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
            } else {
                Serial.println("时区设置后无法获取时间，请检查网络连接");
            }
        } else if (command.length() > 0) {
            Serial.println("未知命令: " + command);
            Serial.println("支持的命令:");
            Serial.println("  test_anim - 测试数字滚动动画");
            Serial.println("  show_time - 显示当前时间信息");
            Serial.println("  set_timezone <offset> - 设置时区 (例如: set_timezone 8 表示UTC+8)");
        }
    }
}

/**
 * 数字滚动动画测试函数
 * 功能：测试数字从当前值变到另一个值再变回来的动画效果
 * 特点：不改变实际数据，仅用于测试动画效果
 */
void testNumberAnimation() {
    // 保存当前的真实数据值
    int originalStars = currentStars;
    int originalForks = currentForks;
    int originalWatchers = currentWatchers;
    
    // 保存当前的动画状态
    int originalAnimatingStars = animatingStars;
    int originalAnimatingForks = animatingForks;
    int originalAnimatingWatchers = animatingWatchers;
    
    Serial.printf("当前数据 - Stars: %d, Forks: %d, Watchers: %d\n", originalStars, originalForks, originalWatchers);
    Serial.printf("当前动画状态 - Stars: %d, Forks: %d, Watchers: %d\n", originalAnimatingStars, originalAnimatingForks, originalAnimatingWatchers);
    
    // 第一阶段：从当前值动画到测试值
    Serial.println("第一阶段：数字增加动画");
    if (animatingStars >= 0) {
        int testStars = animatingStars + 100;  // 增加100个星标
        Serial.printf("Stars动画: %d -> %d\n", animatingStars, testStars);
        animateNumber(stars_count_label, animatingStars, testStars, "");
        animatingStars = testStars;
    }
    
    if (animatingForks >= 0) {
        int testForks = animatingForks + 20;   // 增加20个分支
        Serial.printf("Forks动画: %d -> %d\n", animatingForks, testForks);
        animateNumber(forks_label, animatingForks, testForks, "Forks: ");
        animatingForks = testForks;
    }
    
    if (animatingWatchers >= 0) {
        int testWatchers = animatingWatchers + 10;  // 增加10个关注者
        Serial.printf("Watchers动画: %d -> %d\n", animatingWatchers, testWatchers);
        animateNumber(watchers_label, animatingWatchers, testWatchers, "Watchers: ");
        animatingWatchers = testWatchers;
    }
    
    // 延迟2秒后执行第二阶段
    Serial.println("等待2秒后执行第二阶段...");
    delay(2000);
    
    // 第二阶段：从测试值动画回原始值
    Serial.println("第二阶段：数字恢复动画");
    if (originalAnimatingStars >= 0) {
        Serial.printf("Stars恢复动画: %d -> %d\n", animatingStars, originalAnimatingStars);
        animateNumber(stars_count_label, animatingStars, originalAnimatingStars, "");
        animatingStars = originalAnimatingStars;
    }
    
    if (originalAnimatingForks >= 0) {
        Serial.printf("Forks恢复动画: %d -> %d\n", animatingForks, originalAnimatingForks);
        animateNumber(forks_label, animatingForks, originalAnimatingForks, "Forks: ");
        animatingForks = originalAnimatingForks;
    }
    
    if (originalAnimatingWatchers >= 0) {
        Serial.printf("Watchers恢复动画: %d -> %d\n", animatingWatchers, originalAnimatingWatchers);
        animateNumber(watchers_label, animatingWatchers, originalAnimatingWatchers, "Watchers: ");
        animatingWatchers = originalAnimatingWatchers;
    }
    
    // 确保实际数据值不变
    currentStars = originalStars;
    currentForks = originalForks;
    currentWatchers = originalWatchers;
    
    Serial.println("=== 动画测试完成，数据已恢复原始值 ===");
    Serial.printf("恢复后数据 - Stars: %d, Forks: %d, Watchers: %d\n", currentStars, currentForks, currentWatchers);
}

/**
 * GitHub API数据获取函数
 * 功能：从GitHub REST API获取指定仓库的统计数据
 * 获取数据：Stars数量、Forks数量、Watchers数量
 * 特点：
 * - 使用Bearer Token认证，支持私有仓库
 * - 带进度条的用户体验
 * - 完整的错误处理和状态反馈
 * - JSON数据解析和显示更新
 * - 15秒超时保护
 */
void fetchGitHubData() {
    Serial.println("\n=== 开始获取GitHub数据 ===");
    
    // 检查WiFi连接状态，无网络时直接返回
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("错误: WiFi未连接，无法获取数据");
        updateStatus("WiFi Disconnected", lv_color_hex(0xef4444));
        // 显示网络错误提示框，引导用户去设置
        show_network_error_message_box("Network Error", "WiFi not connected.\nCannot fetch GitHub data.\nTap OK to go to Settings.");
        return;
    }
    
    // 设置获取数据标志，防止其他函数覆盖状态显示
    Serial.println("[DEBUG] 设置 isFetchingData = true");
    isFetchingData = true;
    
    // 只在开机第一次或当前显示为占位符时重置显示
    // 检查当前是否显示占位符（开机第一次或数据无效状态）
    const char* current_stars_text = lv_label_get_text(stars_count_label);
    bool isFirstTime = (strcmp(current_stars_text, "---") == 0);
    
    if (isFirstTime) {
        Serial.println("开机第一次获取数据，保持占位符显示...");
        // 保持占位符显示，动画状态已经是-1
    } else {
        Serial.println("数据更新，保持当前数字显示，准备使用数字滚动动画...");
        // 不重置显示和动画状态，让updateDisplay使用数字到数字的动画
    }
    lv_timer_handler();  // 刷新UI显示
    
    Serial.println("WiFi连接正常，开始数据获取流程...");
    Serial.printf("目标仓库: %s/%s\n", repoOwner, repoName);

    // 更新UI状态，显示数据获取进度
    Serial.println("[DEBUG] 准备调用 updateStatus('Fetching data...')");
    updateStatus("Fetching data...", lv_color_hex(0x3b82f6));
    lv_obj_clear_flag(progress_bar, LV_OBJ_FLAG_HIDDEN);  // 显示进度条
    lv_bar_set_value(progress_bar, 10, LV_ANIM_OFF);      // 设置初始进度10%
    lv_timer_handler();  // 刷新UI显示
    Serial.println("[DEBUG] UI状态已更新: 显示'Fetching data...'在左下角");
    delay(500); // 短暂延时确保UI刷新
    Serial.println("[DEBUG] 开始HTTP请求流程...");

    // 配置HTTPS客户端，准备API请求
    WiFiClientSecure client;
    HTTPClient http;
    String url = "https://api.github.com/repos/" + String(repoOwner) + "/" + String(repoName);
    Serial.println("配置HTTPS客户端...");
    Serial.printf("请求URL: %s\n", url.c_str());
    
    // 配置安全客户端（跳过证书验证）
    client.setInsecure();
    
    http.begin(client, url);  // 设置请求URL
    http.addHeader("Authorization", "Bearer " + String(githubToken));  // 添加认证头
    http.addHeader("User-Agent", "ESP32-GitHub-Display");  // 添加User-Agent（GitHub API要求）
    http.setTimeout(15000);  // 设置15秒超时
    Serial.println("HTTP请求头已配置，超时设置为15秒");

    lv_bar_set_value(progress_bar, 30, LV_ANIM_ON);  // 更新进度到30%
    lv_timer_handler();  // 刷新UI显示
    
    Serial.println("发送HTTP GET请求到GitHub API...");
    int httpCode = http.GET();  // 发送GET请求到GitHub API
    Serial.printf("HTTP响应码: %d\n", httpCode);
    
    lv_bar_set_value(progress_bar, 70, LV_ANIM_ON);  // 更新进度到70%
    lv_timer_handler();  // 刷新UI显示

    if (httpCode == HTTP_CODE_OK) {
        // HTTP请求成功，开始处理响应数据
        Serial.println("HTTP请求成功，开始解析JSON数据...");
        String payload = http.getString();  // 获取响应JSON字符串
        Serial.printf("响应数据长度: %d 字节\n", payload.length());
        
        DynamicJsonDocument doc(2048);      // 创建JSON文档对象（2KB缓冲区）
        DeserializationError error = deserializeJson(doc, payload);  // 解析JSON
        
        if (error == DeserializationError::Ok) {
            // JSON解析成功，提取仓库统计数据
            Serial.println("=== JSON解析调试信息 ===");
            Serial.printf("JSON字段stargazers_count存在: %s\n", doc.containsKey("stargazers_count") ? "是" : "否");
            Serial.printf("JSON字段是否为null: %s\n", doc["stargazers_count"].isNull() ? "是" : "否");
            
            currentStars = doc["stargazers_count"].as<int>();    // 明确转换为int类型
            currentForks = doc["forks_count"].as<int>();         // 明确转换为int类型
            currentWatchers = doc["subscribers_count"].as<int>(); // 明确转换为int类型
            
            Serial.println("JSON解析成功，获取到的数据:");
            Serial.printf("  Stars: %d (原始值: %s)\n", currentStars, doc["stargazers_count"].as<String>().c_str());
            Serial.printf("  Forks: %d\n", currentForks);
            Serial.printf("  Watchers: %d\n", currentWatchers);
            Serial.printf("=== 准备保存星标数据: %d ===\n", currentStars);
            
            // 检查是否是新的一天并更新昨日数据
            checkAndUpdateDailyData();
            
            // 保存星标数据到文件系统
            Serial.printf("=== 调用saveStarData前，currentStars值: %d ===\n", currentStars);
            saveStarData(currentStars);
            Serial.printf("=== 调用saveStarData后，currentStars值: %d ===\n", currentStars);
            
            // 保存增长数据（包括增加和减少的变化）
            saveGrowthData(currentStars);
            
            // 更新上次保存的星标数并持久化
            lastSavedStars = currentStars;
            save_settings();  // 立即保存到NVS，确保重启后数据不丢失
            
            // 完成进度条到100%
            lv_bar_set_value(progress_bar, 100, LV_ANIM_ON);
            lv_timer_handler();
            
            // 更新UI状态为成功，并设置成功状态显示时间
            updateStatus("Update successful", lv_color_hex(0x10b981));
            showingUpdateSuccess = true;     // 标记正在显示更新成功状态
            updateSuccessTime = millis();    // 记录成功时间，用于3秒后恢复正常显示
            lv_label_set_text(time_label, "Last Upd: <1 min");  // 立即更新时间显示
            
            // 清除重试状态（数据获取成功）
            waitingForRetry = false;
            Serial.println("[DEBUG] 数据获取成功，清除重试状态");
            
            Serial.println("UI状态已更新为'Update successful'");
        } else {
            // JSON解析失败
            Serial.printf("JSON解析失败: %s\n", error.c_str());
            updateStatus("Data parse error", lv_color_hex(0xef4444));
        }
    } else {
        // HTTP请求失败，显示错误码
        Serial.printf("HTTP请求失败，错误码: %d\n", httpCode);
        
        String errorTitle = "GitHub Error";
        String errorMessage = "";
        String statusMessage = "";
        
        switch (httpCode) {
            case -1:
                statusMessage = "Connection Failed";
                errorMessage = "Network connection failed.\nPossible causes:\n• WiFi signal is weak or unstable\n• DNS resolution failed\n• Internet connection is down\n• Router/firewall blocking requests\n\nTrying to reconnect in 10 seconds...";
                // 设置重试机制
                waitingForRetry = true;
                retryTime = millis() + RETRY_INTERVAL;
                Serial.println("[DEBUG] 设置重试机制，1秒后重新获取数据");
                break;
            case 404:
                statusMessage = "Repository not found";
                errorMessage = "Repository not found.\nPlease check if the owner and repository name are correct.\nVerify your GitHub settings.";
                // 清除重试状态（配置错误，不需要重试）
                waitingForRetry = false;
                break;
            case 401:
                statusMessage = "Unauthorized access";
                errorMessage = "GitHub Token is invalid or expired.\nPlease check your token in settings.\nEnsure the token has proper permissions.";
                // 清除重试状态（认证错误，不需要重试）
                waitingForRetry = false;
                break;
            case 403:
                statusMessage = "Access forbidden";
                errorMessage = "GitHub Token is invalid or API rate limit exceeded.\nPlease check your token or try again later.\nVerify token permissions.";
                // 清除重试状态（权限错误，不需要重试）
                waitingForRetry = false;
                break;
            case 429:
                statusMessage = "Rate limit exceeded";
                errorMessage = "API requests too frequent.\nPlease wait a moment and try again.\nConsider using a personal access token.";
                // 清除重试状态（频率限制，不需要重试）
                waitingForRetry = false;
                break;
            case 500:
            case 502:
            case 503:
            case 504:
                statusMessage = "Server error";
                errorMessage = "GitHub server is temporarily unavailable.\nPlease try again later.\nThis is not an issue with your settings.";
                break;
            default:
                statusMessage = "HTTP Error: " + String(httpCode);
                errorMessage = "Failed to fetch GitHub data.\nHTTP Error: " + String(httpCode) + "\nPlease check your network and settings.";
                break;
        }
        
        updateStatus(statusMessage.c_str(), lv_color_hex(0xef4444));
        show_network_error_message_box(errorTitle.c_str(), errorMessage.c_str());
    }
    
    http.end();          // 关闭HTTP连接，释放资源
    Serial.println("HTTP连接已关闭");
    
    // 清除获取数据标志
    Serial.println("[DEBUG] 设置 isFetchingData = false");
    isFetchingData = false;
    
    updateDisplay();     // 更新显示界面的数据
    updateProgressBar(); // 恢复进度条正常显示
    
    Serial.println("UI界面数据已更新");
    Serial.println("=== GitHub数据获取完成 ===\n");
}

/**
 * 数字动画回调函数
 * 功能：在动画过程中更新标签显示的数值
 * 参数：var - 指向标签对象的指针，val - 当前动画值
 */
static void number_anim_cb(void* var, int32_t val) {
    lv_obj_t* label = (lv_obj_t*)var;
    
    // 根据标签类型设置不同的格式
    if (label == stars_count_label) {
        lv_label_set_text_fmt(label, "%d", (int)val);
    } else if (label == forks_label) {
        lv_label_set_text_fmt(label, "Forks: %d", (int)val);
    } else if (label == watchers_label) {
        lv_label_set_text_fmt(label, "Watchers: %d", (int)val);
    }
}

/**
 * 从占位符到数字的动画回调函数（优化版）
 * 功能：创建从占位符到实际数字的平滑过渡动画
 * 参数：var - 指向标签对象的指针，val - 当前动画值(0-100)
 * 优化特点：
 * - 使用透明度渐变实现更平滑的过渡
 * - 添加数字逐步显现效果
 * - 减少闪烁，提升视觉体验
 */
static void placeholder_to_number_anim_cb(void* var, int32_t val) {
    lv_obj_t* label = (lv_obj_t*)var;
    
    // 计算透明度和进度
    lv_opa_t opacity = LV_OPA_COVER;
    
    if (val < 30) {
        // 第一阶段：占位符淡出效果 (0-30%)
        opacity = LV_OPA_COVER - (val * LV_OPA_COVER / 30);
        
        if (label == stars_count_label) {
            lv_label_set_text(label, "---");
        } else if (label == forks_label) {
            lv_label_set_text(label, "Forks: --");
        } else if (label == watchers_label) {
            lv_label_set_text(label, "Watchers: --");
        }
        
        // 设置透明度
        lv_obj_set_style_text_opa(label, opacity, 0);
        
    } else if (val < 70) {
        // 第二阶段：加载动画 (30-70%)
        int dots = ((val - 30) / 10) % 4; // 0-3个点的循环
        char loading_text[20];
        
        if (label == stars_count_label) {
            snprintf(loading_text, sizeof(loading_text), "%.*s", dots, "...");
            lv_label_set_text(label, loading_text);
        } else if (label == forks_label) {
            snprintf(loading_text, sizeof(loading_text), "Forks: %.*s", dots, "...");
            lv_label_set_text(label, loading_text);
        } else if (label == watchers_label) {
            snprintf(loading_text, sizeof(loading_text), "Watchers: %.*s", dots, "...");
            lv_label_set_text(label, loading_text);
        }
        
        // 透明度逐渐恢复
        opacity = LV_OPA_30 + ((val - 30) * (LV_OPA_COVER - LV_OPA_30) / 40);
        lv_obj_set_style_text_opa(label, opacity, 0);
        
    } else {
        // 第三阶段：数字淡入效果 (70-100%)
        int target_value = 0;
        
        // 获取目标数值
        if (label == stars_count_label) {
            target_value = currentStars;
        } else if (label == forks_label) {
            target_value = currentForks;
        } else if (label == watchers_label) {
            target_value = currentWatchers;
        }
        
        // 数字逐步显现效果
        int progress = val - 70; // 0-30的进度
        int current_display = (target_value * progress) / 30;
        
        if (label == stars_count_label) {
            lv_label_set_text_fmt(label, "%d", current_display);
        } else if (label == forks_label) {
            lv_label_set_text_fmt(label, "Forks: %d", current_display);
        } else if (label == watchers_label) {
            lv_label_set_text_fmt(label, "Watchers: %d", current_display);
        }
        
        // 完全不透明
        lv_obj_set_style_text_opa(label, LV_OPA_COVER, 0);
    }
}

/**
 * 数字滚动动画函数
 * 功能：创建从旧值到新值的平滑滚动动画
 * 参数：label - 目标标签，from - 起始值，to - 目标值，prefix - 前缀文本
 */
void animateNumber(lv_obj_t* label, int from, int to, const char* prefix) {
    if (from == to) return;  // 值相同时不需要动画
    
    // 创建动画对象
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, label);
    lv_anim_set_values(&anim, from, to);
    lv_anim_set_time(&anim, 800);  // 动画持续时间800ms
    lv_anim_set_exec_cb(&anim, number_anim_cb);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);  // 使用缓出动画曲线
    lv_anim_start(&anim);
}

/**
 * 从占位符到数字的动画函数（优化版）
 * 功能：创建从占位符到实际数字的平滑过渡动画
 * 参数：label - 目标标签
 * 优化特点：
 * - 使用更平滑的动画曲线
 * - 优化动画时长以配合三阶段效果
 * - 添加动画完成后的清理逻辑
 */
void animatePlaceholderToNumber(lv_obj_t* label) {
    // 创建动画对象
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, label);
    lv_anim_set_values(&anim, 0, 100);  // 0-100的进度值
    lv_anim_set_time(&anim, 1800);  // 动画持续时间1.8秒，配合三阶段效果
    lv_anim_set_exec_cb(&anim, placeholder_to_number_anim_cb);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_out);  // 使用缓出动画曲线，更自然
    
    // 添加动画完成回调，确保最终状态正确
     lv_anim_set_ready_cb(&anim, [](lv_anim_t* a) {
         lv_obj_t* label = (lv_obj_t*)a->var;
         // 确保透明度完全恢复
         lv_obj_set_style_text_opa(label, LV_OPA_COVER, 0);
         
         // 确保显示最终正确的数值
         if (label == stars_count_label && currentStars >= 0) {
             lv_label_set_text_fmt(label, "%d", currentStars);
         } else if (label == forks_label && currentForks >= 0) {
             lv_label_set_text_fmt(label, "Forks: %d", currentForks);
         } else if (label == watchers_label && currentWatchers >= 0) {
             lv_label_set_text_fmt(label, "Watchers: %d", currentWatchers);
         }
     });
    
    lv_anim_start(&anim);
}

/**
 * 检查是否是新的一天并更新昨日数据
 * 功能：检测日期变化，将当前星标数设为昨日星标数
 * 特点：使用NVS存储上次检查的日期，确保跨重启的日期检测
 */
void checkAndUpdateDailyData() {
    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    
    // 获取当前日期（年月日）
    int currentDay = timeinfo.tm_year * 10000 + timeinfo.tm_mon * 100 + timeinfo.tm_mday;
    
    // 从NVS读取上次检查的日期
    preferences.begin("gh-display", false);
    int lastCheckDay = preferences.getInt("lastCheckDay", 0);
    
    if (lastCheckDay != currentDay && currentStars >= 0) {
        // 新的一天，更新昨日数据
        Serial.printf("检测到新的一天，更新昨日星标数: %d -> %d\n", yesterdayStars, currentStars);
        yesterdayStars = currentStars;
        preferences.putInt("yesterdayStars", yesterdayStars);
        preferences.putInt("lastCheckDay", currentDay);
    }
    
    preferences.end();
}

/**
 * 更新星标变化量显示
 * 功能：显示星标增长量，使用简单的+数字格式
 * 特点：有增长显示红色+号，无昨日数据显示-号
 */
void updateStarsDelta() {
    if (currentStars >= 0 && yesterdayStars >= 0) {
        starsDelta = currentStars - yesterdayStars;
        
        if (starsDelta >= 0) {
            lv_label_set_text_fmt(stars_delta_label, "+%d", starsDelta);
            lv_obj_set_style_text_color(stars_delta_label, lv_color_hex(0xef4444), 0); // 红色
        }
        
        Serial.printf("星标变化量: %d (今日: %d, 昨日: %d)\n", starsDelta, currentStars, yesterdayStars);
    } else if (currentStars >= 0) {
        // 有当前数据但没有昨日数据，显示-号表示无历史数据
        lv_label_set_text(stars_delta_label, "-");
        lv_obj_set_style_text_color(stars_delta_label, lv_color_hex(0x9ca3af), 0); // 灰色
    } else {
        // 没有任何数据
        lv_label_set_text(stars_delta_label, "");
    }
}

/**
 * 更新主显示界面的数据
 * 功能：将获取到的GitHub仓库数据更新到UI标签上
 * 包括：Stars数量、Forks数量、Watchers数量
 * 特点：对无效数据显示"---"或"--"占位符，有效数据使用动画更新
 */
void updateDisplay() {
    // 更新Stars数量显示
    if (currentStars >= 0) {
        if (animatingStars >= 0) {
            // 使用动画从旧值滚动到新值
            animateNumber(stars_count_label, animatingStars, currentStars, "");
        } else {
            // 从占位符状态到数字，使用特殊动画
            animatePlaceholderToNumber(stars_count_label);
        }
        animatingStars = currentStars;  // 记录当前值用于下次动画
    } else {
        lv_label_set_text(stars_count_label, "---");  // 数据无效时显示占位符
        animatingStars = -1;  // 重置动画状态
    }
    
    // 更新星标变化量显示
    updateStarsDelta();
    
    // 更新Forks数量显示
    if (currentForks >= 0) {
        if (animatingForks >= 0) {
            // 使用动画从旧值滚动到新值
            animateNumber(forks_label, animatingForks, currentForks, "Forks: ");
        } else {
            // 从占位符状态到数字，使用特殊动画
            animatePlaceholderToNumber(forks_label);
        }
        animatingForks = currentForks;  // 记录当前值用于下次动画
    } else {
        lv_label_set_text(forks_label, "Forks: --");  // 数据无效时显示占位符
        animatingForks = -1;  // 重置动画状态
    }
    
    // 更新Watchers数量显示
    if (currentWatchers >= 0) {
        if (animatingWatchers >= 0) {
            // 使用动画从旧值滚动到新值
            animateNumber(watchers_label, animatingWatchers, currentWatchers, "Watchers: ");
        } else {
            // 从占位符状态到数字，使用特殊动画
            animatePlaceholderToNumber(watchers_label);
        }
        animatingWatchers = currentWatchers;  // 记录当前值用于下次动画
    } else {
        lv_label_set_text(watchers_label, "Watchers: --");  // 数据无效时显示占位符
        animatingWatchers = -1;  // 重置动画状态
    }
    
    updateTimeDisplay();  // 同时更新时间显示
    lv_timer_handler();   // 刷新UI显示
}

/**
 * 智能时间显示更新函数
 * 功能：根据WiFi状态和数据更新时间，智能显示相应的时间信息
 * 显示逻辑：
 * - WiFi未连接：显示"Go to Settings"
 * - 刚更新成功或不到1分钟：显示"Last Upd: <1min"
 * - 接近下次更新时间：显示"Update soon"
 * - 正常情况：显示"Last Upd: X mins"
 * - 从未更新过：显示"Last Upd: --"
 */
void updateTimeDisplay() {
    // 手动刷新期间跳过时间显示更新，避免干扰倒计时
    if (isManualRefreshing) {
        return;
    }
    
    // 检查WiFi连接状态，未连接时提示用户进入设置
    if (WiFi.status() != WL_CONNECTED) {
        lv_label_set_text(time_label, "Go to Settings");
        return;
    }
    
    if (lastDataUpdate > 0) {
        // 计算距离上次数据更新的时间（分钟）
        unsigned long timeSinceUpdate = (millis() - lastDataUpdate) / 60000;
        
        // 智能显示逻辑：根据时间间隔显示不同的提示信息
        if (showingUpdateSuccess || timeSinceUpdate < 1) {
            // 正在显示更新成功状态，或刚更新不到1分钟
            lv_label_set_text(time_label, "Last Upd: <1min");
        } else if (timeSinceUpdate >= (UPDATE_INTERVAL / 60000) -1) {
            // 接近下次自动更新时间（29分钟后）
            lv_label_set_text(time_label, "Update soon");
        } else {
            // 正常情况，显示具体的更新时间间隔
            lv_label_set_text_fmt(time_label, "Last Upd: %lu mins", timeSinceUpdate);
        }
    } else {
        // 系统启动后从未成功更新过数据
        lv_label_set_text(time_label, "Last Upd: --");
    }
}

/**
 * 显示当前系统时间函数
 * 功能：获取并显示当前的本地时间（中国标准时间CST）
 * 格式："CST HH:MM"（如：CST 14:30）
 * 特点：仅在成功获取时间时才更新显示
 */
void showCurrentTime() {
    // 如果正在获取数据，不覆盖状态显示
    if (isFetchingData) {
        Serial.println("[DEBUG] showCurrentTime: 跳过，正在获取数据");
        return;
    }
    Serial.println("[DEBUG] showCurrentTime: 开始显示当前时间");
    
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {  // 尝试获取本地时间
        char timeStr[20];
        strftime(timeStr, sizeof(timeStr), "CST %H:%M", &timeinfo);  // 格式化时间字符串
        updateStatus(timeStr, lv_color_hex(0x60a5fa));  // 以蓝色显示时间
    }
}

/**
 * 更新当前时间显示函数
 * 功能：更新标题下方的实时时间显示
 * 格式："HH:MM:SS"（如：14:30:25）
 * 特点：每秒更新一次，显示完整的时分秒
 */
void updateCurrentTimeDisplay() {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {  // 尝试获取本地时间
        char timeStr[10];
        strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);  // 格式化时间字符串
        lv_label_set_text(current_time_label, timeStr);  // 更新时间显示
    } else {
        lv_label_set_text(current_time_label, "--:--:--");  // 无法获取时间时显示占位符
    }
}

/**
 * 更新进度条显示函数
 * 功能：根据距离下次数据更新的剩余时间，动态显示进度条
 * 逻辑：
 * - 手动刷新倒计时：进度从100%每秒递减20%
 * - 刚更新成功：显示100%进度
 * - 正常情况：进度随时间递减（100% -> 0%）
 * - 从未更新：隐藏进度条
 * 特点：5分钟更新周期的可视化倒计时
 */
void updateProgressBar() {
    // 处理手动刷新倒计时
    if (isManualRefreshing) {
        unsigned long elapsed = millis() - manualRefreshStartTime;
        if (elapsed < MANUAL_REFRESH_DURATION) {
            // 计算倒计时进度：从100%线性递减到0%
            int progress = 100 - (elapsed * 100) / MANUAL_REFRESH_DURATION;
            if (progress < 0) progress = 0;  // 确保不会小于0%
            lv_bar_set_value(progress_bar, progress, LV_ANIM_OFF);
            
            // 更新倒计时提示 - 优化更新频率
            static int lastDisplayedSeconds = -1;
            int remainingSeconds = (MANUAL_REFRESH_DURATION - elapsed) / 1000 + 1;
            if (remainingSeconds != lastDisplayedSeconds) {
                char countdownMsg[50];
                snprintf(countdownMsg, sizeof(countdownMsg), "Data will refresh in %d seconds", remainingSeconds);
                updateStatus(countdownMsg, lv_color_hex(0x3b82f6));
                lastDisplayedSeconds = remainingSeconds;
                Serial.printf("倒计时更新: %d 秒\n", remainingSeconds);
            }
            
            // 强制UI刷新确保及时显示
            lv_timer_handler();
        } else {
            // 倒计时结束，开始刷新数据
            Serial.println("手动刷新倒计时结束，开始获取数据");
            isManualRefreshing = false;
            lastDataUpdate = millis();  // 更新时间戳
            fetchGitHubData();  // 执行数据刷新
            
            // 按钮变回蓝色
            lv_obj_set_style_bg_color(touch_test_btn, lv_color_hex(0x007BFF), 0);
            refreshButtonGreen = false;
        }
        return;
    }
    
    if (lastDataUpdate > 0) {
        unsigned long timeSinceUpdate = millis() - lastDataUpdate;  // 计算已过时间
        lv_obj_clear_flag(progress_bar, LV_OBJ_FLAG_HIDDEN);  // 显示进度条
        
        if (showingUpdateSuccess) {
            // 正在显示更新成功状态，进度条显示100%
            lv_bar_set_value(progress_bar, 100, LV_ANIM_OFF);
        } else {
            // 计算剩余时间百分比（从100%递减到0%）
            int progress = 100 - (timeSinceUpdate * 100) / UPDATE_INTERVAL;
            lv_bar_set_value(progress_bar, progress > 0 ? progress : 0, LV_ANIM_OFF);

        }
    } else {
        // 从未更新过数据，隐藏进度条
        lv_obj_add_flag(progress_bar, LV_OBJ_FLAG_HIDDEN);
    }
}

void updateStatus(const char *message, lv_color_t color) {
    Serial.printf("[DEBUG] updateStatus called: '%s'\n", message);
    lv_label_set_text(status_label, message);
    lv_obj_set_style_text_color(status_label, color, 0);
    
    // 强制刷新显示
    lv_obj_invalidate(status_label);  // 标记状态标签需要重绘
    lv_timer_handler();               // 处理LVGL事件
    lv_timer_handler();               // 再次处理确保刷新
    lv_refr_now(NULL);               // 立即刷新显示
    
    Serial.printf("[DEBUG] updateStatus completed, UI force refreshed\n");
}

void initDisplayAndTouch() {
    Serial.println("初始化显示和触摸...");
    tft.init();
    tft.setRotation(1);
    tft.setBrightness(200);

    touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
    touchscreen.begin(touchscreenSPI);
    touchscreen.setRotation(1);

    lv_init();
    lv_display_t *disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_buffers(disp, buf, buf2, sizeof(buf), LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);

    Serial.println("显示和触摸初始化完成。");
}

// ===== 主程序 =====
/**
 * Arduino程序主初始化函数
 * 功能：系统启动时执行一次，完成所有硬件和软件模块的初始化
 * 执行流程：
 * 1. 串口通信初始化
 * 2. 从NVS存储加载用户配置
 * 3. WiFi模块初始化
 * 4. 显示屏和触摸屏初始化
 * 5. 创建所有UI界面
 * 6. 尝试WiFi连接和数据获取
 * 7. 启动定时更新机制
 * 
 * 时区设置说明：
 * 如果您发现日历显示的星期和日期不匹配，请检查时区设置。
 * 在configTime函数中修改第一个参数（时区偏移量）：
 * - 中国时区 (UTC+8): 8 * 3600
 * - 日本时区 (UTC+9): 9 * 3600  
 * - 美国东部 (UTC-5): -5 * 3600
 * - 美国西部 (UTC-8): -8 * 3600
 * - 英国时区 (UTC+0): 0 * 3600
 */
void setup() {
    // 初始化串口通信，波特率115200，用于调试输出
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n===== ESP32 GitHub Stars 显示系统 (带设置功能) =====");

    // 从NVS（非易失性存储）加载用户配置（WiFi凭据、GitHub设置等）
    load_settings();
    
    // 加载天气设置
    loadWeatherSettings();
    
    // 检查是否是新的一天并更新昨日数据
    checkAndUpdateDailyData();
    
    // 初始化WiFi模块，设置为Station模式（客户端模式）
    Serial.println("初始化WiFi模块...");
    WiFi.mode(WIFI_STA);
    delay(100);
    
    // 初始化LittleFS文件系统，用于存储历史数据
    Serial.println("初始化LittleFS文件系统...");
    if (!LittleFS.begin()) {
        Serial.println("LittleFS初始化失败，尝试格式化...");
        if (LittleFS.format()) {
            Serial.println("LittleFS格式化成功，重新初始化...");
            if (LittleFS.begin()) {
                Serial.println("LittleFS重新初始化成功");
            } else {
                Serial.println("LittleFS重新初始化失败！");
            }
        } else {
            Serial.println("LittleFS格式化失败！");
        }
    } else {
        Serial.println("LittleFS初始化成功");
        // 检查文件系统状态
        size_t totalBytes = LittleFS.totalBytes();
        size_t usedBytes = LittleFS.usedBytes();
        Serial.printf("文件系统状态: 总空间=%d字节, 已用=%d字节, 可用=%d字节\n", 
                     totalBytes, usedBytes, totalBytes - usedBytes);
    }

    // *** 新增代码：如果NVS中没有保存过配置，则从secrets.h加载默认值 ***
    if (strlen(ssid) == 0) {
        strncpy(ssid, SECRET_SSID, sizeof(ssid) - 1);
    }
    if (strlen(password) == 0) {
        strncpy(password, SECRET_PASSWORD, sizeof(password) - 1);
    }
    if (strlen(githubToken) == 0) {
        strncpy(githubToken, SECRET_GITHUB_TOKEN, sizeof(githubToken) - 1);
    }
    // ***************************************************************
    
    // 初始化显示屏和触摸屏硬件
    initDisplayAndTouch();
    
    // 创建所有UI界面（必须在主屏幕之前创建，确保界面切换正常）
    create_settings_screen();      // 创建设置主界面
    create_wifi_list_screen();     // 创建WiFi网络列表界面
    create_github_settings_screen(); // 创建GitHub配置界面
    create_chart_screen();         // 创建数据可视化图表界面
    create_weather_screen();       // 创建天气界面
    create_calendar_screen();      // 创建日历界面
    createUI(); // 创建主显示界面

    // 创建功能按钮（设置按钮、触摸测试按钮等）
    setup_settings_button();
#ifdef ENABLE_TOUCH_TEST
    setup_touch_test();  // 仅在启用触摸测试时创建
#endif

    // 加载主屏幕并尝试建立网络连接
    lv_scr_load(main_screen);
    if (connectWiFi()) {
        // WiFi连接成功，配置时区并获取GitHub数据
        // 注意：如果您不在中国时区，请修改时区偏移量
        // UTC+8 (中国): 8 * 3600
        // UTC+0 (格林威治): 0 * 3600  
        // UTC-5 (美国东部): -5 * 3600
        // UTC-8 (美国西部): -8 * 3600
        configTime(8 * 3600, 0, "pool.ntp.org", "time.nist.gov");  // 设置中国时区(UTC+8)
        
        // 等待NTP时间同步
        Serial.println("等待NTP时间同步...");
        struct tm timeinfo;
        int attempts = 0;
        while (!getLocalTime(&timeinfo) && attempts < 10) {
            delay(1000);
            attempts++;
            Serial.printf("NTP同步尝试 %d/10\n", attempts);
        }
        
        if (getLocalTime(&timeinfo)) {
            Serial.println("NTP时间同步成功");
            char timeStr[64];
            strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
            Serial.printf("当前时间: %s\n", timeStr);
            
            // 详细的时间调试信息
            const char* weekdays[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
            Serial.printf("详细时间信息:\n");
            Serial.printf("  年份: %d\n", timeinfo.tm_year + 1900);
            Serial.printf("  月份: %d\n", timeinfo.tm_mon + 1);
            Serial.printf("  日期: %d\n", timeinfo.tm_mday);
            Serial.printf("  星期: %s (tm_wday=%d)\n", weekdays[timeinfo.tm_wday], timeinfo.tm_wday);
            Serial.printf("  时间: %02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        } else {
            Serial.println("NTP时间同步失败，使用默认时间");
        }
        
        // 延时2秒让用户看到"WiFi Connected"状态
        delay(2000);
        
        fetchGitHubData();  // 获取GitHub数据（会显示"Fetching data..."状态）
        lastDataUpdate = millis();  // 记录数据更新时间
        
        // 获取天气数据
        fetchWeatherData();  // 获取天气数据
        lastWeatherUpdate = millis();  // 记录天气数据更新时间
    } else {
        // WiFi连接失败，提示用户进入设置
        updateStatus("Configure WiFi in Settings", lv_color_hex(0xfbbf24));
    }
    
    // 初始化定时器变量，启动各种定时更新机制
    lastTimeUpdate = millis();     // 时间显示更新定时器
    lastProgressUpdate = millis(); // 进度条更新定时器
    updateProgressBar();           // 初始化进度条显示
    Serial.println("\n初始化完成！开始主循环...");
}

/**
 * Arduino程序主循环函数
 * 功能：系统运行期间持续执行，处理所有实时任务和定时更新
 * 主要任务：
 * 1. LVGL图形库事件处理和时钟更新
 * 2. WiFi扫描结果处理和网络列表更新
 * 3. 定时获取GitHub数据（每30分钟）
 * 4. 定时更新时间显示（每分钟）
 * 5. 定时更新进度条（每5秒）
 * 6. WiFi连接状态监控和异常处理
 */
void loop() {
    // 静态变量声明，用于WiFi状态监控
    static bool wasConnected = true;
    
    // 处理串口命令（用于动画测试）
    handleSerialCommands();
    
    // LVGL图形库必需的处理函数，处理UI事件、动画等
    lv_timer_handler();
    lv_tick_inc(1);  // 增加LVGL内部时钟计数
    delay(1);        // 短暂延时，避免CPU占用过高

    unsigned long currentMillis = millis();  // 获取当前时间戳，用于定时任务

    // 处理WiFi网络扫描结果，更新WiFi列表界面
    int n = WiFi.scanComplete();
    if (n >= 0) {
        // WiFi扫描已完成，处理扫描结果
        Serial.printf("WiFi扫描完成，找到 %d 个网络\n", n);
        lv_obj_t* list = lv_obj_get_child(screen_wifi_list, 1);   // 获取WiFi列表控件
        lv_obj_t* label = lv_obj_get_child(screen_wifi_list, 2);  // 获取状态标签控件
        lv_label_set_text(label, "");
        lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);  // 隐藏"扫描中"提示

        if (n == 0) {
            // 未找到任何WiFi网络
            Serial.println("未找到WiFi网络");
            lv_label_set_text(label, "No networks found.");
            lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);  // 显示"未找到网络"提示
        } else {
            // 找到WiFi网络，添加到列表中
            Serial.println("添加WiFi网络到列表:");
            for (int i = 0; i < n; ++i) {
                String ssid = WiFi.SSID(i);
                Serial.printf("  %d: %s (信号强度: %d dBm)\n", i, ssid.c_str(), WiFi.RSSI(i));
                // 为每个WiFi网络创建列表项按钮
                lv_obj_t* btn = lv_list_add_btn(list, LV_SYMBOL_WIFI, ssid.c_str());
                // 设置按钮样式，使其与列表背景协调
                lv_obj_set_style_bg_color(btn, lv_color_hex(0x334155), 0);  // 深灰背景
                lv_obj_set_style_bg_grad_color(btn, lv_color_hex(0x475569), 0);  // 渐变
                lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_VER, 0);
                lv_obj_set_style_text_color(btn, lv_color_hex(0xf1f5f9), 0);  // 浅色文字
                // 为按钮文本设置支持中文的字体
                lv_obj_t* label = lv_obj_get_child(btn, 1); // 获取按钮内的文本标签
                lv_obj_set_style_text_font(label, &lv_font_simsun_16_cjk, 0);
                lv_obj_set_style_text_color(label, lv_color_hex(0xf1f5f9), 0);  // 浅色文字
                lv_obj_add_event_cb(btn, wifi_list_event_cb, LV_EVENT_CLICKED, NULL);
            }
        }
        WiFi.scanDelete();  // 清除扫描结果，释放内存
        Serial.println("WiFi扫描结果处理完成");
    } else if (n == WIFI_SCAN_RUNNING) {
        // WiFi扫描仍在进行中，定期输出状态信息
        static unsigned long lastScanStatus = 0;
        if (millis() - lastScanStatus > 10000) {
            Serial.println("WiFi扫描仍在进行中...");
            lastScanStatus = millis();
        }
    }

    // 处理"更新成功"状态显示的超时（显示3秒后恢复正常状态）
    if (showingUpdateSuccess && currentMillis - updateSuccessTime >= SUCCESS_DISPLAY_TIME) {
        showingUpdateSuccess = false;
        if (!isFetchingData) {
            showCurrentTime();     // 恢复显示当前时间（如果不在获取数据）
        }
        updateProgressBar();   // 恢复进度条正常显示
        updateTimeDisplay();   // 立即更新时间显示为<1min
    }
    
    // 处理重试机制（当HTTP错误码为-1时，10秒后重新获取数据）
    if (waitingForRetry && currentMillis >= retryTime) {
        Serial.println("[DEBUG] 重试时间到，重新获取GitHub数据");
        waitingForRetry = false;
        if (WiFi.status() == WL_CONNECTED && !isFetchingData) {
            fetchGitHubData();
        }
    }
    
    // 定时获取GitHub数据（每30分钟更新一次，仅在WiFi连接时执行）
    if (WiFi.status() == WL_CONNECTED && currentMillis - lastDataUpdate >= UPDATE_INTERVAL) {
        fetchGitHubData();           // 获取最新的GitHub仓库数据
        lastDataUpdate = currentMillis;  // 更新最后数据获取时间
    }
    
    // 定时获取天气数据（每10分钟更新一次，仅在WiFi连接时执行）
    if (WiFi.status() == WL_CONNECTED && currentMillis - lastWeatherUpdate >= WEATHER_UPDATE_INTERVAL) {
        Serial.println("定时更新天气数据");
        fetchWeatherData();          // 获取最新的天气数据
        lastWeatherUpdate = currentMillis;  // 更新最后天气数据获取时间
    }
    
    // 检查是否需要在特定时间自动刷新天气数据（早上8点、中午12点、晚上6点）
    static unsigned long lastWeatherTimeCheck = 0;
    if (WiFi.status() == WL_CONNECTED && currentMillis - lastWeatherTimeCheck >= 60000) { // 每分钟检查一次
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            int currentHour = timeinfo.tm_hour;
            int currentMinute = timeinfo.tm_min;
            
            // 检查是否是指定的刷新时间（8:00, 12:00, 18:00）
            if ((currentHour == 8 || currentHour == 12 || currentHour == 18) && currentMinute == 0) {
                // 检查是否已经在这个小时内刷新过（避免重复刷新）
                static int lastAutoRefreshHour = -1;
                if (lastAutoRefreshHour != currentHour) {
                    Serial.printf("自动天气刷新时间到：%02d:00\n", currentHour);
                    fetchWeatherData();
                    lastWeatherUpdate = currentMillis;
                    lastAutoRefreshHour = currentHour;
                    
                    // 如果当前在天气界面，立即更新显示
                    if (lv_scr_act() == screen_weather && isWeatherDataValid) {
                        updateWeatherDisplay();
                    }
                }
            }
        }
        lastWeatherTimeCheck = currentMillis;
    }
    
    // 定时更新当前时间显示（每秒更新一次）
    static unsigned long lastCurrentTimeUpdate = 0;
    if (currentMillis - lastCurrentTimeUpdate >= 1000) {
        updateCurrentTimeDisplay();  // 更新标题下方的实时时间显示
        lastCurrentTimeUpdate = currentMillis;
    }
    
    // 定时更新时间显示（每分钟更新一次"Last Upd"信息）
    if (currentMillis - lastTimeUpdate >= TIME_UPDATE_INTERVAL) {
        if (!showingUpdateSuccess && !isFetchingData) { 
            showCurrentTime();   // 显示当前系统时间（如果不在显示更新成功状态且不在获取数据）
        }
        updateTimeDisplay();     // 更新"Last Upd"时间显示
        checkAndSaveDailyData(); // 检查并保存每日数据
        lastTimeUpdate = currentMillis;
    }
    
    // 定时更新进度条（手动刷新时每100ms更新一次，正常时每1秒更新一次）
    unsigned long progressUpdateInterval = isManualRefreshing ? 100 : 1000;
    if (currentMillis - lastProgressUpdate >= progressUpdateInterval) {
        updateProgressBar();

        lastProgressUpdate = currentMillis;
    }
    
    // WiFi连接状态监控和异常处理
    static unsigned long lastWiFiCheck = 0;
    bool currentlyConnected = (WiFi.status() == WL_CONNECTED);
    
    // 每30秒检查一次WiFi连接状态
    if (currentMillis - lastWiFiCheck > 30000) {
        if (!currentlyConnected && !isFetchingData && !networkErrorShowing) {
            updateStatus("WiFi Disconnected", lv_color_hex(0xef4444));  // 显示WiFi断开提示
            // 显示网络错误提示框，引导用户去设置
            show_network_error_message_box("Network Error", "WiFi connection lost.\nPlease check your network settings.\nTap OK to go to Settings.");
        }
        lastWiFiCheck = currentMillis;
    }
    
    // 检测WiFi状态变化：从断开到重新连接
    if (!wasConnected && currentlyConnected) {
        // WiFi重新连接后，重置所有相关状态
        lastDataUpdate = 0;          // 重置数据更新时间，触发立即更新
        lastWeatherUpdate = 0;     // 重置天气数据更新时间，触发立即更新
        showingUpdateSuccess = false; // 清除更新成功状态
        updateSuccessTime = 0;       // 重置更新成功时间
        networkErrorShowing = false; // 重置网络错误状态，允许再次显示错误消息框
        // 立即更新显示界面
        fetchGitHubData();
        fetchWeatherData();          // 获取天气数据
        Serial.println("WiFi重新连接，状态已重置");
    }
    
    wasConnected = currentlyConnected;  // 保存当前连接状态，用于下次比较
}

/**
 * 保存星标数据到文件
 * 参数：starCount - 当前星标数量
 */
void saveStarData(int starCount) {
    Serial.println("=== 保存星标数据 ===");
    Serial.printf("=== 接收到的starCount参数值: %d ===\n", starCount);
    
    // 获取当前时间戳
    time_t now;
    time(&now);
    Serial.printf("=== 当前时间戳: %lu ===\n", now);
    
    // 打开文件进行追加写入
    File file = LittleFS.open(DATA_FILE, "a");
    if (!file) {
        Serial.println("无法打开数据文件进行写入");
        return;
    }
    
    // 写入数据：时间戳,星标数量
    Serial.printf("=== 准备写入文件: 时间戳=%lu, 星标=%d ===\n", (unsigned long)now, starCount);
    file.printf("%lu,%d\n", (unsigned long)now, starCount);
    file.close();
    
    Serial.printf("已保存数据: 时间戳=%lu, 星标=%d\n", now, starCount);
    
    // 检查文件大小，如果超过限制则清理旧数据
    file = LittleFS.open(DATA_FILE, "r");
    if (file) {
        size_t fileSize = file.size();
        file.close();
        
        // 如果文件超过10KB，保留最新的50%数据
        if (fileSize > 10240) {
            Serial.println("数据文件过大，清理旧数据...");
            cleanupOldData();
        }
    }
}

/**
 * 清理旧数据，保留最新的一半数据
 */
void cleanupOldData() {
    File file = LittleFS.open(DATA_FILE, "r");
    if (!file) return;
    
    // 读取所有行到临时数组
    String lines[200];  // 最多保存200行
    int lineCount = 0;
    
    while (file.available() && lineCount < 200) {
        lines[lineCount] = file.readStringUntil('\n');
        lineCount++;
    }
    file.close();
    
    // 保留后一半数据
    int keepFrom = lineCount / 2;
    
    // 重写文件
    file = LittleFS.open(DATA_FILE, "w");
    if (file) {
        for (int i = keepFrom; i < lineCount; i++) {
            file.println(lines[i]);
        }
        file.close();
        Serial.printf("数据清理完成，保留了 %d 行数据\n", lineCount - keepFrom);
    }
}

/**
 * 保存每日星标数据
 * 功能：在每天23:59分保存当天的星标数，用于七天数据统计
 */
void saveDailyStarData(int starCount) {
    Serial.println("=== 保存每日星标数据 ===");
    Serial.printf("=== 接收到的starCount参数值: %d ===\n", starCount);
    
    // 获取当前时间戳
    time_t now;
    time(&now);
    Serial.printf("=== 当前时间戳: %lu ===\n", now);
    
    // 打开文件进行追加写入
    File file = LittleFS.open(DAILY_DATA_FILE, "a");
    if (!file) {
        Serial.println("无法打开每日数据文件进行写入");
        return;
    }
    
    // 写入数据：时间戳,星标数量
    Serial.printf("=== 准备写入每日文件: 时间戳=%lu, 星标=%d ===\n", (unsigned long)now, starCount);
    file.printf("%lu,%d\n", (unsigned long)now, starCount);
    file.close();
    
    Serial.printf("已保存每日数据: 时间戳=%lu, 星标=%d\n", now, starCount);
    
    // 检查文件大小，保留最近7天的数据
    file = LittleFS.open(DAILY_DATA_FILE, "r");
    if (file) {
        String lines[10];  // 最多保存10行（超过7天的缓冲）
        int lineCount = 0;
        
        while (file.available() && lineCount < 10) {
            lines[lineCount] = file.readStringUntil('\n');
            lineCount++;
        }
        file.close();
        
        // 如果超过7行，保留最新的7行
        if (lineCount > 7) {
            file = LittleFS.open(DAILY_DATA_FILE, "w");
            if (file) {
                int keepFrom = lineCount - 7;
                for (int i = keepFrom; i < lineCount; i++) {
                    file.println(lines[i]);
                }
                file.close();
                Serial.printf("每日数据清理完成，保留了最近7天数据\n");
            }
        }
    }
}

/**
 * 保存增长数据
 * 功能：保存所有星标变化（增加和减少），用于增长趋势图表显示
 */
void saveGrowthData(int starCount) {
    // 检查是否有变化（只有在有上次数据且数值发生变化时才保存）
    if (lastSavedStars >= 0 && starCount == lastSavedStars) {
        Serial.printf("星标数无变化 (%d -> %d)，跳过保存\n", lastSavedStars, starCount);
        return;
    }
    
    Serial.println("=== 保存增长数据 ===");
    if (lastSavedStars >= 0) {
        int change = starCount - lastSavedStars;
        if (change > 0) {
            Serial.printf("=== 检测到增长: %d -> %d (+%d) ===\n", lastSavedStars, starCount, change);
        } else {
            Serial.printf("=== 检测到减少: %d -> %d (%d) ===\n", lastSavedStars, starCount, change);
        }
    } else {
        Serial.printf("=== 首次保存增长数据: %d ===\n", starCount);
    }
    
    // 获取当前时间戳
    time_t now;
    time(&now);
    
    // 打开文件进行追加写入
    File file = LittleFS.open(GROWTH_DATA_FILE, "a");
    if (!file) {
        Serial.println("无法打开增长数据文件进行写入");
        return;
    }
    
    // 写入数据：时间戳,星标数量
    file.printf("%lu,%d\n", (unsigned long)now, starCount);
    file.close();
    
    Serial.printf("已保存增长数据: 时间戳=%lu, 星标=%d\n", now, starCount);
    
    // 检查文件大小，如果超过限制则清理旧数据
    file = LittleFS.open(GROWTH_DATA_FILE, "r");
    if (file) {
        size_t fileSize = file.size();
        file.close();
        
        // 如果文件超过5KB，保留最新的50%数据
        if (fileSize > 5120) {
            Serial.println("增长数据文件过大，清理旧数据...");
            
            file = LittleFS.open(GROWTH_DATA_FILE, "r");
            if (file) {
                String lines[100];  // 最多保存100行
                int lineCount = 0;
                
                while (file.available() && lineCount < 100) {
                    lines[lineCount] = file.readStringUntil('\n');
                    lineCount++;
                }
                file.close();
                
                // 保留后一半数据
                int keepFrom = lineCount / 2;
                
                file = LittleFS.open(GROWTH_DATA_FILE, "w");
                if (file) {
                    for (int i = keepFrom; i < lineCount; i++) {
                        file.println(lines[i]);
                    }
                    file.close();
                    Serial.printf("增长数据清理完成，保留了 %d 行数据\n", lineCount - keepFrom);
                }
            }
        }
    }
}

/**
 * 检查并保存每日数据
 * 功能：在每天23:59分自动调用，保存当天的星标数
 */
void checkAndSaveDailyData() {
    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    
    // 检查是否是23:59分
    if (timeinfo.tm_hour == 23 && timeinfo.tm_min == 59) {
        // 检查是否已经保存过今天的数据（避免重复保存）
        time_t todayStart = now - (timeinfo.tm_hour * 3600 + timeinfo.tm_min * 60 + timeinfo.tm_sec);
        
        if (lastDailySaveTime < todayStart && currentStars >= 0) {
            Serial.println("检测到23:59分，保存每日数据");
            saveDailyStarData(currentStars);
            lastDailySaveTime = now;
            
            // 保存到NVS以便重启后记住
            preferences.begin("gh-display", false);
            preferences.putULong("lastDailySave", lastDailySaveTime);
            preferences.end();
        }
    }
}

/**
 * 加载图表数据
 * 功能：从文件系统读取历史数据并根据显示模式处理
 */
void loadChartData() {
    Serial.println("=== 加载图表数据 ===");
    
    // 清空现有数据
    chart_data_count = 0;
    
    // 根据显示模式选择数据文件
    const char* dataFile;
    if (chart_view_mode == 1) {
        dataFile = DAILY_DATA_FILE;  // 每天数据使用专门的每日文件
        Serial.println("使用每日数据文件");
    } else if (chart_view_mode == 2) {
        dataFile = GROWTH_DATA_FILE;  // 增长数据使用专门的增长文件
        Serial.println("使用增长数据文件");
    } else {
        dataFile = DATA_FILE;  // 每次获取数据使用原始文件
        Serial.println("使用原始数据文件");
    }
    
    File file = LittleFS.open(dataFile, "r");
    if (!file) {
        Serial.printf("数据文件不存在: %s\n", dataFile);
        return;
    }
    
    // 临时存储所有数据点
    struct DataPoint {
        time_t timestamp;
        int stars;
    };
    
    DataPoint allData[200];  // 最多读取200个数据点
    int totalCount = 0;
    
    // 读取所有数据
    while (file.available() && totalCount < 200) {
        String line = file.readStringUntil('\n');
        line.trim();
        
        if (line.length() > 0) {
            int commaIndex = line.indexOf(',');
            if (commaIndex > 0) {
                allData[totalCount].timestamp = line.substring(0, commaIndex).toInt();
                allData[totalCount].stars = line.substring(commaIndex + 1).toInt();
                totalCount++;
            }
        }
    }
    file.close();
    
    Serial.printf("读取到 %d 个数据点\n", totalCount);
    
    if (totalCount == 0) {
        Serial.println("没有可用的历史数据");
        return;
    }
    
    // 根据显示模式处理数据
    if (chart_view_mode == 0) {
        // 模式0：每次获取数据（显示最近10个点）
        int startIndex = (totalCount > 10) ? totalCount - 10 : 0;
        for (int i = startIndex; i < totalCount; i++) {
            chart_data[chart_data_count] = allData[i].stars;
            chart_data_count++;
        }
    } else if (chart_view_mode == 1) {
        // 模式1：每天数据（直接使用每日文件的数据，显示最近7天）
        int startIndex = (totalCount > 7) ? totalCount - 7 : 0;
        for (int i = startIndex; i < totalCount; i++) {
            chart_data[chart_data_count] = allData[i].stars;
            chart_data_count++;
        }
    } else if (chart_view_mode == 2) {
        // 模式2：增长数据（直接使用增长文件的数据，显示最近10个点）
        int startIndex = (totalCount > 10) ? totalCount - 10 : 0;
        for (int i = startIndex; i < totalCount; i++) {
            chart_data[chart_data_count] = allData[i].stars;
            chart_data_count++;
        }
    }
    
    Serial.printf("处理后的数据点数量: %d\n", chart_data_count);
}

/**
 * 更新图表显示
 * 功能：将数据应用到LVGL图表控件，并添加数据点标签和横坐标标签
 */
void updateChartDisplay() {
    if (chart_obj == NULL || chart_series == NULL) {
        Serial.println("图表对象未初始化");
        return;
    }
    
    // 清除现有数据点标签
    lv_obj_t* parent = lv_obj_get_parent(chart_obj);
    lv_obj_t* child = lv_obj_get_child(parent, 0);
    while (child != NULL) {
        lv_obj_t* next = lv_obj_get_child(parent, lv_obj_get_index(child) + 1);
        if (child != chart_obj && lv_obj_check_type(child, &lv_label_class)) {
            // 检查是否是数据点标签或坐标轴标签（通过用户数据标识）
            void* user_data = lv_obj_get_user_data(child);
            if (user_data == (void*)0x1234 || user_data == (void*)0x5678) {
                lv_obj_del(child);
            }
        }
        child = next;
    }
    
    // 启动数据动画而不是直接设置数据
    if (chart_data_count > 0) {
        startChartAnimation();
        
        // 延迟创建标签，等待动画开始
        lv_timer_t* label_timer = lv_timer_create(chart_labels_timer_cb, 50, NULL);  // 50ms后创建标签
        
        return;  // 动画期间不执行后续的标签创建代码
    }
    
    // 自动调整Y轴范围
    if (chart_data_count > 0) {
        int minVal = chart_data[0];
        int maxVal = chart_data[0];
        
        for (int i = 1; i < chart_data_count; i++) {
            if (chart_data[i] < minVal) minVal = chart_data[i];
            if (chart_data[i] > maxVal) maxVal = chart_data[i];
        }
        
        // 添加一些边距
        int range = maxVal - minVal;
        int margin = range * 0.1;  // 10%边距
        if (margin < 1) margin = 1;
        
        lv_chart_set_range(chart_obj, LV_CHART_AXIS_PRIMARY_Y, minVal - margin, maxVal + margin);
        
        // 添加数据点标签
        lv_area_t chart_area;
        lv_obj_get_coords(chart_obj, &chart_area);
        int chart_width = lv_area_get_width(&chart_area);
        int chart_height = lv_area_get_height(&chart_area);
        
        // 定义容器边界变量（避免重复声明）
        lv_coord_t container_left_narrow = chart_area.x1 - 10;  // 数据点标签边界
        lv_coord_t container_right_narrow = chart_area.x2 + 10;
        lv_coord_t container_top = chart_area.y1 - 30;
        lv_coord_t container_left_wide = chart_area.x1 - 20;    // 横坐标标签边界
        lv_coord_t container_right_wide = chart_area.x2 + 20;
        
        for (int i = 0; i < chart_data_count; i++) {
            // 计算数据点在图表中的位置
            int x_pos;
            if (chart_data_count == 1) {
                // 只有一个数据点时，放在图表中央
                x_pos = chart_area.x1 + chart_width / 2;
            } else {
                // 多个数据点时，均匀分布
                x_pos = chart_area.x1 + (i * chart_width) / (chart_data_count - 1);
            }
            int y_pos;
            if (maxVal - minVal + 2 * margin == 0) {
                // 防止除零错误，当所有数据点相同时
                y_pos = chart_area.y1 + chart_height / 2;
            } else {
                y_pos = chart_area.y2 - ((chart_data[i] - minVal + margin) * chart_height) / (maxVal - minVal + 2 * margin);
            }
            
            // 创建数据点数值标签 - 根据数据相同情况决定上下位置
            lv_obj_t* value_label = lv_label_create(parent);
            lv_label_set_text_fmt(value_label, "%d", chart_data[i]);
            lv_obj_set_style_text_font(value_label, &lv_font_montserrat_10, 0);
            lv_obj_set_style_text_color(value_label, lv_color_hex(0xfbbf24), 0);
            // 获取标签尺寸以实现精确居中
            lv_obj_update_layout(value_label);
            lv_coord_t label_width = lv_obj_get_width(value_label);
            lv_coord_t label_height = lv_obj_get_height(value_label);
            
            // 检查是否所有数据相同（形成直线）
            bool all_same = true;
            for (int j = 1; j < chart_data_count; j++) {
                if (chart_data[j] != chart_data[0]) {
                    all_same = false;
                    break;
                }
            }
            
            // 获取父容器坐标用于转换绝对坐标到相对坐标
            lv_area_t parent_area;
            lv_obj_get_coords(parent, &parent_area);
            
            // 精确居中对齐：x坐标减去标签宽度的一半
            lv_coord_t value_label_x = x_pos - label_width / 2;
            lv_coord_t value_label_y;
            
            if (all_same) {
                // 所有数据相同时，交替显示在上方和下方
                if (i % 2 == 0) {
                    value_label_y = y_pos - label_height - 10;  // 数据点上方2像素
                } else {
                    value_label_y = y_pos + 1;  // 数据点下方2像素
                }
            } else {
                // 数据不同时，统一显示在上方
                value_label_y = y_pos - label_height - 2;  // 数据点上方2像素
            }
            
            // 转换为相对于父容器的坐标
            value_label_x -= parent_area.x1;
            value_label_y -= parent_area.y1;
            
            // 限制标签位置不超出图表容器范围（相对坐标）
            lv_coord_t container_left_rel = container_left_narrow - parent_area.x1;
            lv_coord_t container_right_rel = container_right_narrow - parent_area.x1;
            lv_coord_t container_top_rel = container_top - parent_area.y1;
            lv_coord_t container_bottom_rel = (chart_area.y2 + 30) - parent_area.y1;
            
            if (value_label_x < container_left_rel) value_label_x = container_left_rel;
            if (value_label_x + label_width > container_right_rel) value_label_x = container_right_rel - label_width;
            if (value_label_y < container_top_rel) value_label_y = container_top_rel;
            if (value_label_y + label_height > container_bottom_rel) value_label_y = container_bottom_rel - label_height;
            
            lv_obj_set_pos(value_label, value_label_x, value_label_y);
            lv_obj_set_user_data(value_label, (void*)0x1234);  // 标记为数据点标签
            
            // 创建横坐标标签
            lv_obj_t* x_label = lv_label_create(parent);
            lv_obj_set_style_text_font(x_label, &lv_font_montserrat_10, 0);
            lv_obj_set_style_text_color(x_label, lv_color_hex(0x94a3b8), 0);
            lv_obj_set_user_data(x_label, (void*)0x5678);  // 标记为坐标轴标签
            
            // 根据图表模式设置横坐标标签文本
            if (chart_view_mode == 0) {
                // 每次获取模式：最新的显示"now"，其他显示序号
                if (i == chart_data_count - 1) {
                    lv_label_set_text(x_label, "now");
                } else {
                    lv_label_set_text_fmt(x_label, "-%d", chart_data_count - 1 - i);
                }
            } else if (chart_view_mode == 1) {
                // 每日数据模式：显示星期
                const char* weekdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
                // 假设最新的是今天，向前推算
                time_t now = time(NULL);
                struct tm* timeinfo = localtime(&now);
                int today_weekday = timeinfo->tm_wday;
                int day_offset = chart_data_count - 1 - i;
                int weekday = (today_weekday - day_offset + 7) % 7;
                lv_label_set_text(x_label, weekdays[weekday]);
            } else if (chart_view_mode == 2) {
                // 增长数据模式：显示增长点序号
                lv_label_set_text_fmt(x_label, "G%d", i + 1);
            }
            
            // 获取横坐标标签尺寸以实现精确居中
            lv_obj_update_layout(x_label);
            lv_coord_t x_label_width = lv_obj_get_width(x_label);
            // 精确居中对齐：x坐标减去标签宽度的一半
            lv_coord_t x_label_x = x_pos - x_label_width / 2;
            lv_coord_t x_label_y = chart_area.y2 - 2;  // 显示在图表下方，更靠近图表
            
            // 转换为相对于父容器的坐标
            x_label_x -= parent_area.x1;
            x_label_y -= parent_area.y1;
            
            // 限制标签位置不超出图表容器范围（相对坐标）
            lv_coord_t container_left_wide_rel = container_left_wide - parent_area.x1;
            lv_coord_t container_right_wide_rel = container_right_wide - parent_area.x1;
            
            if (x_label_x < container_left_wide_rel) x_label_x = container_left_wide_rel;
            if (x_label_x + x_label_width > container_right_wide_rel) x_label_x = container_right_wide_rel - x_label_width;
            
            lv_obj_set_pos(x_label, x_label_x, x_label_y);
        }
    }
    
    lv_obj_invalidate(chart_obj);  // 刷新图表显示
}

/**
 * 星星卡片长按事件回调函数
 * 功能：处理星星卡片的长按事件，切换到图表界面
 */
static void stars_card_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_LONG_PRESSED) {
        Serial.println("星星卡片被长按，切换到图表界面");
        
        // 创建图表界面（如果尚未创建）
        if (screen_chart == NULL) {
            create_chart_screen();
        }
        
        // 加载图表数据
        loadChartData();
        
        // 更新图表显示
        updateChartDisplay();
        
        // 切换到图表界面（加快动画速度）
        lv_scr_load_anim(screen_chart, LV_SCR_LOAD_ANIM_MOVE_LEFT, 100, 0, false);
        
        // 隐藏设置和刷新按钮
        control_buttons_visibility(screen_chart);
    }
}

/**
 * 图表界面返回按钮事件回调函数
 * 功能：从图表界面返回到主界面
 */
/**
 * 图表界面返回按钮事件回调函数
 * 功能：处理图表界面的返回操作，清理图表内存并切换回主界面
 * 参数：e - LVGL事件对象
 */
static void chart_back_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_CLICKED) {
        Serial.println("图表界面返回按钮被点击，开始清理图表内存");
        
        // 先切换回主界面（加快动画速度）
        lv_scr_load_anim(main_screen, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 100, 0, false);
        
        // 恢复设置和刷新按钮显示
        control_buttons_visibility(main_screen);
        
        // 延迟清理图表界面，避免删除活动屏幕
        lv_timer_t * cleanup_timer = lv_timer_create([](lv_timer_t * timer) {
            // 清理图表数据
            chart_data_count = 0;
            memset(chart_data, 0, sizeof(chart_data));
            
            // 重置图表对象指针
            chart_obj = NULL;
            chart_series = NULL;
            
            // 删除图表界面以释放内存
            if (screen_chart != NULL) {
                lv_obj_del(screen_chart);
                screen_chart = NULL;
                Serial.println("图表界面内存已清理");
            }
            
            // 删除定时器
            lv_timer_del(timer);
        }, 150, NULL);
        
        // 设置定时器只执行一次
        lv_timer_set_repeat_count(cleanup_timer, 1);
    }
}

/**
 * 图表模式切换事件回调函数
 * 功能：切换图表的显示模式（每次获取/每天/增长）
 */
static void chart_mode_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_CLICKED) {
        // 切换到下一个模式
        chart_view_mode = (chart_view_mode + 1) % 3;
        
        Serial.printf("切换图表模式到: %d\n", chart_view_mode);
        
        // 重新加载和显示数据
        loadChartData();
        updateChartDisplay();
        
        // 更新模式按钮文本
        lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
        lv_obj_t* label = lv_obj_get_child(btn, 0);
        
        switch (chart_view_mode) {
            case 0:
                lv_label_set_text(label, "Each Fetch");
                break;
            case 1:
                lv_label_set_text(label, "Daily Data");
                break;
            case 2:
                lv_label_set_text(label, "Growth Data");
                break;
        }
    }
}