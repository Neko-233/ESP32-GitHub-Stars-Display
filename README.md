# ESP32 GitHub 星标数显示器

这是一个基于 `ESP32-2432S028R` (俗称“廉价黄屏”) 开发板的 GitHub 仓库信息显示器。它能够通过 WiFi 连接到 GitHub API，实时获取并显示指定仓库的 **星标 (Stars)**、**复刻 (Forks)** 和 **关注者 (Watchers)** 数量。

项目拥有一个基于 **LVGL** 图形库构建的现代化触摸界面，允许用户直接在屏幕上完成所有配置，无需修改代码和重新编译。

![image](https://github.com/user-attachments/assets/6470316a-c315-4138-9b5e-e440f90ab6a2)


## ✨ 主要功能

* **实时数据显示**: 通过 GitHub API 实时获取并展示仓库核心数据。

* **全触摸操作**: 提供一个完整的图形用户界面（GUI），支持多页面导航和参数设置。

* **WiFi 管理**: 内置 WiFi 网络扫描、连接和状态监控功能。

* **持久化存储**: 使用 ESP32 的 NVS（非易失性存储），断电后自动保存 WiFi 和 GitHub 配置信息。

* **自动更新**: 支持设置定时任务，自动刷新并显示最新的仓库数据。

* **安全的配置管理**: 将 WiFi 密码和 GitHub Token 等敏感信息与主代码分离，保护用户隐私。

* **现代化的UI**: 基于 LVGL 设计，包含进度条、图标、多屏幕切换动画等元素，提供流畅的用户体验。

## 硬件要求

* **主控板**: **ESP32-2432S028R**

* **显示驱动**: ILI9341

* **触摸驱动**: XPT2046

*理论上，任何使用相同驱动芯片和引脚配置的 ESP32 开发板都可以运行此项目。*

## ⚙️ 环境配置与依赖

本项目基于 **Arduino IDE** 进行开发。在编译上传前，请确保你已完成以下环境配置：

### 1. 下载项目文件

点击本页面右上角的 `Code` -> `Download ZIP` 下载本项目，并解压到你的 Arduino 项目文件夹中。

### 2. 安装 ESP32 开发板支持

在 Arduino IDE 的“首选项”中，将以下 URL 添加到“附加开发板管理器网址”中：
`https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
然后通过“开发板管理器”搜索并安装 `esp32`。

### 3. 安装必要的库

请通过 Arduino IDE 的 **“库管理器”** 搜索并安装以下所有库的最新版本：

| **库名称** | **作者** | 
| --- | --- |
| `LovyanGFX` | lovyan03 | 
| `LVGL` | lvgl | 
| `XPT2046_Touchscreen` | Paul Stoffregen | 
| `ArduinoJson` | Benoit Blanchon | 

### 4. 创建你的私密配置文件 (重要！)

为了保护你的个人信息，本项目使用一个独立的 `secrets.h` 文件来存放你的 WiFi 密码和 GitHub Token。**此文件不会被上传到 GitHub。**

1. 在你的项目文件夹 (与 `GitHub_Display.ino` 同级) 下，**手动创建一个名为 `secrets.h` 的文件**。

2. 将以下模板内容复制到你刚创建的 `secrets.h` 文件中：

   ```cpp
   // secrets.h
   // 这个文件专门用于存放你的个人敏感信息。
   // 重要：此文件不应该被上传到GitHub。
   
   #ifndef SECRETS_H
   #define SECRETS_H
   
   // 在这里填入你的WiFi名称
   const char* SECRET_SSID = "你的WiFi名";
   
   // 在这里填入你的WiFi密码
   const char* SECRET_PASSWORD = "你的WiFi密码";
   
   // 在这里填入你的GitHub个人访问令牌 (PAT)
   // 你可以在 [https://github.com/settings/tokens](https://github.com/settings/tokens) 生成它
   const char* SECRET_GITHUB_TOKEN = "ghp_xxxxxxxxxxxxxxxxxxxx";
   
   #endif // SECRETS_H
   ```

3. 将文件中的 `"你的WiFi名"`, `"你的WiFi密码"` 等占位符替换为你自己的真实信息。

### 5. 确认项目文件结构

完成以上步骤后，请确保你的项目文件夹中至少包含以下文件：

```
GitHub_Display/
├── GitHub_Display.ino      (主代码文件)
├── secrets.h               (你的私密配置文件)
├── lv_conf.h               (LVGL的配置文件)
└── font_awesome_16.cpp     (自定义图标字体)
```

## 🚀 编译与使用

1. **编译与上传**:

   * 用 Arduino IDE 打开 `GitHub_Display.ino` 文件。

   * 连接你的 ESP32 开发板，在“工具”菜单中选择正确的开发板型号和端口。

   * 点击“上传”按钮。

2. **设备端配置**:

   * **首次启动**: 设备会自动使用 `secrets.h` 里的信息进行连接。

   * **后续修改**: 你可以随时点击屏幕右上角的 **齿轮图标** `⚙️` 进入设置菜单，覆盖原有的 WiFi 或 GitHub 仓库设置。新的设置将被保存在设备的闪存中。

## 📝 许可证

本项目采用 [MIT License](https://www.google.com/search?q=LICENSE) 开源。
