## 项目为AIO固件配套上位机
Holocubic_AIO开源地址 https://github.com/ClimbSnail/HoloCubic_AIO

[^_^]:
	![AIO_TOOL](Image/holocubic_aio_tool.png)

![AIO_TOOL](https://gitee.com/ClimbSnailQ/Project_Image/raw/master/OtherProject/holocubic_aio_tool.png)

## 架构设计

### AIO 工具流程图

![AIO 工具流程图](../Image/AIO-Tool-flowchart.png)

上图展示了 AIO 工具应用程序的架构和数据流，显示了不同模块之间的交互方式。

## 快速开始

### 前置要求
- [uv](https://github.com/astral-sh/uv) - 快速的 Python 包管理器（推荐）

### 一键安装（推荐）

使用自动化脚本快速设置环境：

```bash
# Windows PowerShell
.\setup.ps1
```

### 手动安装

#### 方法 1: 使用 uv（推荐）

```bash
# 1. 安装 uv (如果尚未安装)
# Windows (PowerShell)
powershell -c "irm https://astral.sh/uv/install.ps1 | iex"

# macOS/Linux
curl -LsSf https://astral.sh/uv/install.sh | sh

# 2. 创建虚拟环境
uv venv

# 3. 安装所有依赖（包括本地的 esptool）
uv pip install -r requirements.txt

# 4. 运行应用
uv run python CubicAIO_Tool.py
```

#### 方法 2: 传统方式

```bash
# 1. 创建虚拟环境
python -m venv venv

# 2. 激活虚拟环境
# Windows
venv\Scripts\activate
# macOS/Linux
source venv/bin/activate

# 3. 安装所有依赖（包括本地的 esptool）
pip install -r requirements.txt

# 4. 运行应用
python CubicAIO_Tool.py
```

### 依赖说明

本项目的 `requirements.txt` 包含：
- **运行时依赖**: pillow, requests, pyserial
- **esptool v4.1**: 从本地 `esptool_v41/` 目录安装
- **构建工具**: pyinstaller（用于打包可执行文件）

所有 esptool 的依赖（bitstring, cryptography, ecdsa, reedsolo）会自动安装。

## 重要问题
本工程包含了上位机所有代码及资源文件，但唯独缺少视频转化工具`ffmpeg`（文件太大），需要转化功能的可以自行访问`ffmpeg`原项目地址 https://github.com/FFmpeg/FFmpeg 下载，把其中的`ffmpeg.exe`文件放在本工程的根目录下即可。

或者使用包管理器安装（推荐）：
```bash
# Windows (Chocolatey)
choco install ffmpeg -y

# macOS (Homebrew)
brew install ffmpeg

# Linux (Ubuntu/Debian)
sudo apt install ffmpeg
```

## 打包成可执行程序

### 使用 spec 文件（推荐）

本项目包含优化的 `CubicAIO_Tool.spec` 文件，可以正确打包所有依赖（包括 esptool）：

```bash
# 使用 uv（推荐）
uv run pyinstaller CubicAIO_Tool.spec

# 或清理后重新构建
uv run pyinstaller --clean CubicAIO_Tool.spec
```

### 快速打包（不推荐）

```bash
# 使用 uv
uv run pyinstaller --icon ./image/holo_256.ico -w -F CubicAIO_Tool.py

# 传统方式
pyinstaller --icon ./image/holo_256.ico -w -F CubicAIO_Tool.py
```

**⚠️ 注意**: 快速打包可能无法正确包含 esptool 模块，建议使用 `.spec` 文件。

**参数说明：**
- `--icon ./image/holo_256.ico` - 设置应用程序图标
- `-w` - 不显示控制台窗口（仅GUI）
- `-F` - 打包成单个可执行文件

**输出位置：** `dist/CubicAIO_Tool.exe`

## 故障排除

### "No module named 'esptool'" 错误

如果遇到此错误：

1. **检查 esptool 是否已安装**:
   ```bash
   uv pip list | findstr esptool  # Windows
   uv pip list | grep esptool     # macOS/Linux
   ```

2. **重新安装所有依赖**:
   ```bash
   uv pip install --force-reinstall -r requirements.txt
   ```

3. **重新构建可执行文件**:
   ```bash
   uv run pyinstaller --clean CubicAIO_Tool.spec
   ```

### 其他常见问题

- **虚拟环境激活失败**: 确保使用正确的激活命令（见上方安装说明）
- **依赖安装失败**: 尝试升级 pip/uv 到最新版本
- **打包失败**: 确保所有依赖都已正确安装

## 开发笔记

#### 关于烧录
对ESP32进行开发完后，烧录需要提取四个文件，其中包含两个启动引导文件`bootloader_qio_80m.bin`、`boot_app0.bin`，一个flash划分文件`partitions.bin`和一个固件文件`firmware.bin`(在本工程里名为`HoloCubic_AIO固件_vX.X.X.bin`)。https://github.com/ClimbSnail/HoloCubic_AIO/releases/tag/v2.1.0%E5%9B%BA%E4%BB%B6


###### 下面说下这些文件的存放位置以及烧录地址（以Windows为例）：
1. `bootloader_qio_80m.bin`的位置为PlatformIO安装目录下的`.platformio\packages\framework-arduinoespressif32\tools\sdk\bin`目录下面,它的对应的烧录地址为0X1000。
2. `boot_app0.bin`的位置为PlatformIO安装目录下的`platformio\packages\framework-arduinoespressif32\tools\partitions`目录下面，它对应的烧录地址为0xe000
3. `partitions.bin`的位置为代码工程目录下的.pioenvs\[board]目录下面,它对应的烧录地址为0x8000。同时platformio\packages\framework-arduinoespressif32\tools\partitions目录下面的`partitions.csv`为编译的分区配置文件，会根据版型选择的不同有所不同，可以使用Excel打开进行编辑，然后在编译器内使用PIO进行重新编译即可，同时他也可以使用PIO包里面带的`gen_esp32part.py`脚本进行编译与反编译，操作方法为：python C:\SPB_Data\.platformio\packages\framework-arduinoespressif32\tools\gen_esp32part.py --verify xxx.csv xxx.bin(后面填写csv文件或者bin文件存放的位置，这里是将csv转换成bin，如果将位置对换，则可以将bin转换成csv),它的对应的烧录地址为0X8000。
4. `firmware.bin`的位置为代码工程目录下的`.pioenvs\[board]`目录下面，这个就是代码编译出来的固件，它对应的烧录地址为0x10000，如果分区文件未做修改的话（人为修改，或者更换编译平台），更新固件或者重新烧录只在对应地址开始需要烧录这一个文件即可。此文件手动命名为`HoloCubic_AIO固件_vX.X.X.bin`，由于经常随着源码的更新而更新。,它的对应的烧录地址为0x10000。

#### 烧录参考脚本
1. python tool-esptoolpy\esptool.py --port COM7 --baud 921600 write_flash -fm dio -fs 4MB 0x1000 bootloader_qio_80m.bin 0x00008000 partitions.bin 0x0000e000 boot_app0.bin 0x00010000 HoloCubic_AIO固件_v1.3.bin
2. 清空flash命令 python tool-esptoolpy\esptool.py erase_flash

可用波特率为：
* 115200
* 230400
* 460800
* 576000
* 921600
* 1152000


#### 图片转化开发要点
https://lvgl.io/assets/images/logo_lvgl.png

利用lvgl的官方转换器 https://lvgl.io/tools/imageconverter 图片则可以转换成（True color with alpha 选择Binary RGB565）bin文件存储到SD卡中

## 项目结构

```
AIO_Tool/
├── CubicAIO_Tool.py          # 主程序入口
├── CubicAIO_Tool.spec         # PyInstaller 配置文件（包含 esptool 配置）
├── requirements.txt           # Python 依赖列表（包括本地 esptool）
├── setup.ps1                  # Windows 自动化安装脚本
├── esptool_v41/              # 本地 esptool v4.1 包
├── page/                      # UI 页面模块
│   ├── download_debug.py     # 下载调试页面（使用 esptool）
│   ├── videotool.py          # 视频工具
│   ├── images_converter.py   # 图片转换器
│   └── ...
├── util/                      # 工具模块
├── image/                     # 图标资源
├── base_bin/                  # 基础固件文件
└── dist/                      # 构建输出目录
    └── CubicAIO_Tool.exe     # 最终可执行文件
```

## 致谢
* 固件下载工具 https://github.com/espressif/esptool
* 视频转码工具 https://github.com/FFmpeg/FFmpeg
* LVGL离线转换工具 https://github.com/W-Mai/lvgl_image_converter


