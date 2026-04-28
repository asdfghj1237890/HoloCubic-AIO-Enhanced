# -*- coding: utf-8 -*-
################################################################################
#
# Internationalization (i18n) module for HoloCubic AIO Tool
# Supports: Simplified Chinese, Traditional Chinese, and English
#
################################################################################

import json
import os

from util.logger import get_logger

logger = get_logger(__name__)


class I18n:
    """
    Internationalization handler
    """
    
    # Language codes
    LANG_ZH_CN = "zh_CN"  # Simplified Chinese
    LANG_ZH_TW = "zh_TW"  # Traditional Chinese
    LANG_EN_US = "en_US"  # English
    
    # Translation dictionary
    TRANSLATIONS = {
        "zh_CN": {
            # Main window
            "app_title": "HoloCubic_AIO 工具",
            
            # Tab names
            "tab_download_debug": "下载调试",
            "tab_setting": "参数设置",
            "tab_file_manager": "文件管理",
            "tab_image_converter": "图片转换",
            "tab_video_converter": "视频转码",
            "tab_screen_share": "屏幕分享",
            "tab_help": "帮助",
            "tab_tool_settings": "工具设置",
            
            # Tool Settings page
            "tool_settings_title": "工具设置",
            "language_label": "语言 / Language:",
            "language_zh_cn": "简体中文",
            "language_zh_tw": "繁體中文",
            "language_en_us": "English",
            "language_tip": "选择工具界面的显示语言",
            "restart_tip": "注意: 更改语言后需要重启工具才能完全生效",
            "apply_button": "应用",
            "cancel_button": "取消",
            "success_title": "成功",
            "language_changed": "语言设置已更改\n请重启工具以应用新语言",
            
            # Download Debug page
            "serial_settings": "串口设置",
            "firmware_flash": "固件刷写",
            "operation_log": "操作日志",
            "serial_receive": "串口接收",
            "port_number": "端口号",
            "baud_rate": "波特率",
            "open_serial": "打开串口",
            "close_serial": "关闭串口",
            "refresh": "刷新",
            "start_download": "开始下载",
            "cancel_download": "取消下载",
            "clear_flash": "清空Flash",
            "select_file": "选择文件",
            "latest_version": "最新版本",
            "unknown": "未知",
            
            # Device Settings page
            "wifi_settings": "WIFI设置",
            "system_settings": "系统设置",
            "weather_settings": "天气设置",
            "other_settings": "其他设置",
            "read_settings": "读取设置",
            "write_settings": "写入设置",
            
            # Common
            "ok": "确定",
            "cancel": "取消",
            "yes": "是",
            "no": "否",
            "success": "成功",
            "error": "错误",
            "warning": "警告",
            "info": "信息",
            
            # Download Debug page - detailed
            "data_bit": "数据位",
            "check_bit": "校验位",
            "stop_bit": "停止位",
            "no_check": "无校验",
            "odd_check": "奇校验",
            "even_check": "偶校验",
            "zero_check": "0校验",
            "one_check": "1校验",
            "one_bit": "1位",
            "one_half_bit": "1.5位",
            "two_bit": "2位",
            "reboot": "重启",
            "choose_bootloader": "选择Bootloader的bin文件",
            "choose_partitions": "选择partitions的bin文件",
            "choose_boot_app0": "选择boot_app0的bin文件",
            "choose_firmware": "选择user_data的bin文件(AIO固件必选项)",
            "aio_latest_version": "AIO最新版本",
            "clear_chip": "清空芯片",
            "flash_firmware": "刷写固件",
            "cancel_flash": "取消刷写",
            "select_bin_file": "选择一个bin文件",
            "flashing": "刷写中.....",
            "flash_success": "刷写固件成功！",
            
            # Device Settings page - detailed  
            "ssid": "SSID",
            "password": "密码",
            "ssid_1": "SSID 1",
            "password_1": "密码 1",
            "ssid_2": "SSID 2",
            "password_2": "密码 2",
            "backlight": "背光亮度",
            "rotation": "屏幕旋转",
            "auto_mpu": "自动MPU",
            "city_name": "城市名称",
            "weather_key": "天气密钥",
            
            # File Manager page
            "ip_address": "ip地址",
            "port": "端口",
            "download": "下载",
            "rename": "重命名",
            "delete": "删除",
            "properties": "属性",
            "upload_file": "上传文件",
            "new_folder": "新建文件夹",
            "file_operations": "文件操作",
            "folder_operations": "文件夹操作",
            "sd_card_files": "内存卡文件",
            "get_button": "获取",
            
            # Common operations
            "browse": "浏览",
            "save": "保存",
            "open": "打开",
            "close": "关闭",
            "clear": "清空",
            "reset": "重置",
            "send": "发送",
            "receive": "接收",
            "connect": "连接",
            "disconnect": "断开",
            
            # Image Converter page
            "jpg_output": "JPG格式输出",
            "color_format": "ColorFormat",
            "output_format": "OutputFormat",
            "resolution": "分辨率（宽x高）",
            "width_placeholder": "宽",
            "height_placeholder": "高",
            "select_images": "选择要转化的图片路径",
            "select_button": "选择",
            "start_convert": "开始转化",
            "click_to_convert": "点击转化",
            "converting": "正在转化",
            "convert_complete": "转化完成",
            "select_images_title": "选择若干个图片",
            "image_files": "Image",
            "all_files": "所有文件",
            "image_converter_info": '''
        本功能为LVGL图片转化工具  输入任意分辨率图片，转成你所指定的分辨率的图片。
        可以同时选择多张图片，进行批量转换。转化完毕的照片存在本软件同级目录的OutFile文件夹下。
        注：OutFile/Cache为缓存目录，可自行删除。

        若转为存在内存卡中的jpg照片请勾选："JPG格式输出"

        若转为存在内存卡中的bin照片请去掉"JPG格式输出"勾选，后选择：
            ColorFormat：CF_TRUE_COLOR_ALPHA    OutputFormat：Binary_565

        若转为存在Flash固件中的数组代码请去掉"JPG格式输出"勾选，后选择：
            ColorFormat：CF_INDEXED_4_BIT    OutputFormat：C_array
        ''',
            
            # Video Converter page
            "select_video": "选择视频",
            "output_path": "输出路径",
            "start_conversion": "开始转换",
            "converting_video": "正在转换",
            "output_settings": "输出设置",
            "default_option": "默认",
            "custom_option": "自定义",
            "fps": "帧率(fps)",
            "quality": "质量",
            "format": "格式",
            "select_video_title": "选择一个视频文件",
            "common_formats": "常用格式",
            
            # Help page
            "help_info": '''
        本上位机专门针对HoloCubic AIO固件开发，其中固件烧录功能兼容其他第三方固件。

        加入AIO的QQ群获取相关学习资料。
        ①群号755143193 ②群号860112883 ③群号676732387 ④群号749207817
        注：目前4个2000人群均已满，为减少"群费"主推加入"AIO的QQ频道"，最新
        固件优先更新在QQ频道内部。（频道号 f7nut5r3p4）

        功能持续补充完善中，本上位机的所有源码均开源，欢迎大家学习，
        也欢迎加入到AIO固件的开发队列。

        观看演示链接：
            https://www.bilibili.com/video/BV1wS4y1R7YF?p=1

        了解Holocubic AIO固件
            请访问 https://github.com/ClimbSnail/HoloCubic_AIO （最新版本）
            或者 https://gitee.com/ClimbSnailQ/HoloCubic_AIO

        了解Holocubic硬件开源方案
            请访问 https://github.com/peng-zhihui/HoloCubic

        统一资料入口 https://share.weiyun.com/alCGrwxQ
        ''',
        },
        
        "zh_TW": {
            # Main window
            "app_title": "HoloCubic_AIO 工具",
            
            # Tab names
            "tab_download_debug": "下載除錯",
            "tab_setting": "參數設定",
            "tab_file_manager": "檔案管理",
            "tab_image_converter": "圖片轉換",
            "tab_video_converter": "影片轉碼",
            "tab_screen_share": "螢幕分享",
            "tab_help": "說明",
            "tab_tool_settings": "工具設定",
            
            # Tool Settings page
            "tool_settings_title": "工具設定",
            "language_label": "語言 / Language:",
            "language_zh_cn": "简体中文",
            "language_zh_tw": "繁體中文",
            "language_en_us": "English",
            "language_tip": "選擇工具介面的顯示語言",
            "restart_tip": "注意：變更語言後需要重新啟動工具才能完全生效",
            "apply_button": "套用",
            "cancel_button": "取消",
            "success_title": "成功",
            "language_changed": "語言設定已變更\n請重新啟動工具以套用新語言",
            
            # Download Debug page
            "serial_settings": "序列埠設定",
            "firmware_flash": "韌體燒錄",
            "operation_log": "操作記錄",
            "serial_receive": "序列埠接收",
            "port_number": "連接埠",
            "baud_rate": "傳輸速率",
            "open_serial": "開啟序列埠",
            "close_serial": "關閉序列埠",
            "refresh": "重新整理",
            "start_download": "開始下載",
            "cancel_download": "取消下載",
            "clear_flash": "清空Flash",
            "select_file": "選擇檔案",
            "latest_version": "最新版本",
            "unknown": "未知",
            
            # Device Settings page
            "wifi_settings": "WiFi 設定",
            "system_settings": "系統設定",
            "weather_settings": "天氣設定",
            "other_settings": "其他設定",
            "read_settings": "讀取設定",
            "write_settings": "寫入設定",
            
            # Common
            "ok": "確定",
            "cancel": "取消",
            "yes": "是",
            "no": "否",
            "success": "成功",
            "error": "錯誤",
            "warning": "警告",
            "info": "資訊",
            
            # Download Debug page - detailed
            "data_bit": "資料位元",
            "check_bit": "同位檢查",
            "stop_bit": "停止位元",
            "no_check": "無檢查",
            "odd_check": "奇同位",
            "even_check": "偶同位",
            "zero_check": "0 檢查",
            "one_check": "1 檢查",
            "one_bit": "1 位元",
            "one_half_bit": "1.5 位元",
            "two_bit": "2 位元",
            "reboot": "重新開機",
            "choose_bootloader": "選擇 Bootloader 的 bin 檔案",
            "choose_partitions": "選擇 partitions 的 bin 檔案",
            "choose_boot_app0": "選擇 boot_app0 的 bin 檔案",
            "choose_firmware": "選擇 user_data 的 bin 檔案（AIO 韌體必選）",
            "aio_latest_version": "AIO 最新版本",
            "clear_chip": "清空晶片",
            "flash_firmware": "燒錄韌體",
            "cancel_flash": "取消燒錄",
            "select_bin_file": "選擇一個 bin 檔案",
            "flashing": "燒錄中...",
            "flash_success": "韌體燒錄成功！",
            
            # Device Settings page - detailed
            "ssid": "SSID",
            "password": "密碼",
            "ssid_1": "SSID 1",
            "password_1": "密碼 1",
            "ssid_2": "SSID 2",
            "password_2": "密碼 2",
            "backlight": "背光亮度",
            "rotation": "螢幕旋轉",
            "auto_mpu": "自動 MPU",
            "city_name": "城市名稱",
            "weather_key": "天氣金鑰",
            
            # File Manager page
            "ip_address": "IP 位址",
            "port": "連接埠",
            "download": "下載",
            "rename": "重新命名",
            "delete": "刪除",
            "properties": "內容",
            "upload_file": "上傳檔案",
            "new_folder": "新增資料夾",
            "file_operations": "檔案操作",
            "folder_operations": "資料夾操作",
            "sd_card_files": "記憶卡檔案",
            "get_button": "取得",
            
            # Common operations
            "browse": "瀏覽",
            "save": "儲存",
            "open": "開啟",
            "close": "關閉",
            "clear": "清除",
            "reset": "重設",
            "send": "傳送",
            "receive": "接收",
            "connect": "連線",
            "disconnect": "中斷連線",
            
            # Image Converter page
            "jpg_output": "JPG 格式輸出",
            "color_format": "ColorFormat",
            "output_format": "OutputFormat",
            "resolution": "解析度（寬 x 高）",
            "width_placeholder": "寬",
            "height_placeholder": "高",
            "select_images": "選擇要轉換的圖片路徑",
            "select_button": "選擇",
            "start_convert": "開始轉換",
            "click_to_convert": "點擊轉換",
            "converting": "轉換中",
            "convert_complete": "轉換完成",
            "select_images_title": "選擇多張圖片",
            "image_files": "圖片",
            "all_files": "所有檔案",
            "image_converter_info": '''
        本功能為 LVGL 圖片轉換工具，可輸入任意解析度的圖片，轉換成您指定的解析度。
        可以同時選擇多張圖片進行批次轉換。轉換完成的圖片會儲存在本軟體同級目錄的 OutFile 資料夾下。
        注意：OutFile/Cache 為快取目錄，可自行刪除。

        若要轉換為記憶卡中的 jpg 圖片，請勾選：「JPG 格式輸出」

        若要轉換為記憶卡中的 bin 圖片，請取消勾選「JPG 格式輸出」，並選擇：
            ColorFormat：CF_TRUE_COLOR_ALPHA    OutputFormat：Binary_565

        若要轉換為 Flash 韌體中的陣列程式碼，請取消勾選「JPG 格式輸出」，並選擇：
            ColorFormat：CF_INDEXED_4_BIT    OutputFormat：C_array
        ''',
            
            # Video Converter page
            "select_video": "選擇影片",
            "output_path": "輸出路徑",
            "start_conversion": "開始轉換",
            "converting_video": "轉換中",
            "output_settings": "輸出設定",
            "default_option": "預設",
            "custom_option": "自訂",
            "fps": "影格率 (fps)",
            "quality": "品質",
            "format": "格式",
            "select_video_title": "選擇一個影片檔案",
            "common_formats": "常用格式",
            
            # Help page
            "help_info": '''
        本電腦端工具專門針對 HoloCubic AIO 韌體開發，其中韌體燒錄功能相容其他第三方韌體。

        加入 AIO 的 QQ 群組取得相關學習資料。
        ①群號 755143193 ②群號 860112883 ③群號 676732387 ④群號 749207817
        注意：目前 4 個 2000 人群組均已額滿，為減少「群費」主推加入「AIO 的 QQ 頻道」，
        最新韌體會優先在 QQ 頻道內部更新。（頻道號 f7nut5r3p4）

        功能持續補充完善中，本電腦端工具的所有原始碼均為開源，歡迎大家學習，
        也歡迎加入 AIO 韌體的開發行列。

        觀看示範影片：
            https://www.bilibili.com/video/BV1wS4y1R7YF?p=1

        了解 Holocubic AIO 韌體
            請造訪 https://github.com/ClimbSnail/HoloCubic_AIO （最新版本）
            或者 https://gitee.com/ClimbSnailQ/HoloCubic_AIO

        了解 Holocubic 硬體開源方案
            請造訪 https://github.com/peng-zhihui/HoloCubic

        統一資料入口 https://share.weiyun.com/alCGrwxQ
        ''',
        },
        
        "en_US": {
            # Main window
            "app_title": "HoloCubic_AIO Tool",
            
            # Tab names
            "tab_download_debug": "Download & Debug",
            "tab_setting": "Device Settings",
            "tab_file_manager": "File Manager",
            "tab_image_converter": "Image Converter",
            "tab_video_converter": "Video Converter",
            "tab_screen_share": "Screen Share",
            "tab_help": "Help",
            "tab_tool_settings": "Tool Settings",
            
            # Tool Settings page
            "tool_settings_title": "Tool Settings",
            "language_label": "Language / 语言:",
            "language_zh_cn": "简体中文",
            "language_zh_tw": "繁體中文",
            "language_en_us": "English",
            "language_tip": "Select the display language for the tool interface",
            "restart_tip": "Note: The tool needs to be restarted for language changes to take full effect",
            "apply_button": "Apply",
            "cancel_button": "Cancel",
            "success_title": "Success",
            "language_changed": "Language settings have been changed\nPlease restart the tool to apply the new language",
            
            # Download Debug page
            "serial_settings": "Serial Settings",
            "firmware_flash": "Firmware Flash",
            "operation_log": "Operation Log",
            "serial_receive": "Serial Receive",
            "port_number": "Port",
            "baud_rate": "Baud Rate",
            "open_serial": "Open Serial",
            "close_serial": "Close Serial",
            "refresh": "Refresh",
            "start_download": "Start Download",
            "cancel_download": "Cancel Download",
            "clear_flash": "Clear Flash",
            "select_file": "Select File",
            "latest_version": "Latest Version",
            "unknown": "Unknown",
            
            # Device Settings page
            "wifi_settings": "WiFi Settings",
            "system_settings": "System Settings",
            "weather_settings": "Weather Settings",
            "other_settings": "Other Settings",
            "read_settings": "Read Settings",
            "write_settings": "Write Settings",
            
            # Common
            "ok": "OK",
            "cancel": "Cancel",
            "yes": "Yes",
            "no": "No",
            "success": "Success",
            "error": "Error",
            "warning": "Warning",
            "info": "Info",
            
            # Download Debug page - detailed
            "data_bit": "Data Bit",
            "check_bit": "Parity",
            "stop_bit": "Stop Bit",
            "no_check": "None",
            "odd_check": "Odd",
            "even_check": "Even",
            "zero_check": "Mark",
            "one_check": "Space",
            "one_bit": "1 bit",
            "one_half_bit": "1.5 bits",
            "two_bit": "2 bits",
            "reboot": "Reboot",
            "choose_bootloader": "Select Bootloader bin file",
            "choose_partitions": "Select partitions bin file",
            "choose_boot_app0": "Select boot_app0 bin file",
            "choose_firmware": "Select firmware bin file (Required for AIO)",
            "aio_latest_version": "AIO Latest Version",
            "clear_chip": "Clear Chip",
            "flash_firmware": "Flash Firmware",
            "cancel_flash": "Cancel Flash",
            "select_bin_file": "Select a bin file",
            "flashing": "Flashing.....",
            "flash_success": "Flash firmware successfully!",
            
            # Device Settings page - detailed
            "ssid": "SSID",
            "password": "Password",
            "ssid_1": "SSID 1",
            "password_1": "Password 1",
            "ssid_2": "SSID 2",
            "password_2": "Password 2",
            "backlight": "Backlight",
            "rotation": "Rotation",
            "auto_mpu": "Auto MPU",
            "city_name": "City Name",
            "weather_key": "Weather Key",
            
            # File Manager page
            "ip_address": "IP Address",
            "port": "Port",
            "download": "Download",
            "rename": "Rename",
            "delete": "Delete",
            "properties": "Properties",
            "upload_file": "Upload File",
            "new_folder": "New Folder",
            "file_operations": "File Operations",
            "folder_operations": "Folder Operations",
            "sd_card_files": "SD Card Files",
            "get_button": "Get",
            
            # Common operations
            "browse": "Browse",
            "save": "Save",
            "open": "Open",
            "close": "Close",
            "clear": "Clear",
            "reset": "Reset",
            "send": "Send",
            "receive": "Receive",
            "connect": "Connect",
            "disconnect": "Disconnect",
            
            # Image Converter page
            "jpg_output": "JPG Format Output",
            "color_format": "ColorFormat",
            "output_format": "OutputFormat",
            "resolution": "Resolution (W x H)",
            "width_placeholder": "Width",
            "height_placeholder": "Height",
            "select_images": "Select image path for conversion",
            "select_button": "Select",
            "start_convert": "Start Convert",
            "click_to_convert": "Click to Convert",
            "converting": "Converting",
            "convert_complete": "Conversion Complete",
            "select_images_title": "Select one or more images",
            "image_files": "Image",
            "all_files": "All Files",
            "image_converter_info": '''
        This is an LVGL image conversion tool. Input images at any resolution and convert them to your specified resolution.
        Multiple images can be selected for batch conversion. Converted images are stored in the OutFile folder in the same directory as this software.
        Note: OutFile/Cache is a cache directory and can be deleted freely.

        To convert to JPG images for storage on SD card, check: "JPG Format Output"

        To convert to BIN images for storage on SD card, uncheck "JPG Format Output" and select:
            ColorFormat: CF_TRUE_COLOR_ALPHA    OutputFormat: Binary_565

        To convert to array code for storage in Flash firmware, uncheck "JPG Format Output" and select:
            ColorFormat: CF_INDEXED_4_BIT    OutputFormat: C_array
        ''',
            
            # Video Converter page
            "select_video": "Select Video",
            "output_path": "Output Path",
            "start_conversion": "Start Conversion",
            "converting_video": "Converting",
            "output_settings": "Output Settings",
            "default_option": "Default",
            "custom_option": "Custom",
            "fps": "Frame Rate (fps)",
            "quality": "Quality",
            "format": "Format",
            "select_video_title": "Select a video file",
            "common_formats": "Common Formats",
            
            # Help page
            "help_info": '''
        This PC tool is specifically developed for HoloCubic AIO firmware, with firmware flashing functionality compatible with other third-party firmwares.

        Join AIO's QQ groups to get related learning materials.
        ①Group: 755143193 ②Group: 860112883 ③Group: 676732387 ④Group: 749207817
        Note: Currently all four 2000-member groups are full. To reduce "group fees", we recommend joining the "AIO QQ Channel" where
        the latest firmware is updated first. (Channel ID: f7nut5r3p4)

        Features are continuously being added and improved. All source code of this PC tool is open source, welcome to learn,
        and welcome to join the AIO firmware development team.

        Watch demonstration video:
            https://www.bilibili.com/video/BV1wS4y1R7YF?p=1

        Learn about Holocubic AIO firmware
            Visit https://github.com/ClimbSnail/HoloCubic_AIO (latest version)
            Or https://gitee.com/ClimbSnailQ/HoloCubic_AIO

        Learn about Holocubic hardware open source solution
            Visit https://github.com/peng-zhihui/HoloCubic

        Unified information portal https://share.weiyun.com/alCGrwxQ
        ''',
        }
    }
    
    _instance = None
    _current_language = LANG_ZH_CN
    _config_file = "tool_config.json"
    
    def __new__(cls):
        if cls._instance is None:
            cls._instance = super(I18n, cls).__new__(cls)
            cls._instance._load_language_preference()
        return cls._instance
    
    def _load_language_preference(self):
        """Load language preference from config file"""
        try:
            if os.path.exists(self._config_file):
                with open(self._config_file, 'r', encoding='utf-8') as f:
                    config = json.load(f)
                    saved_lang = config.get('language', self.LANG_ZH_CN)
                    if saved_lang in self.TRANSLATIONS:
                        self._current_language = saved_lang
        except Exception as e:
            logger.error("Failed to load language preference: %s", e)
            self._current_language = self.LANG_ZH_CN
    
    def save_language_preference(self, language):
        """Save language preference to config file"""
        try:
            config = {}
            if os.path.exists(self._config_file):
                with open(self._config_file, 'r', encoding='utf-8') as f:
                    config = json.load(f)
            
            config['language'] = language
            
            with open(self._config_file, 'w', encoding='utf-8') as f:
                json.dump(config, f, ensure_ascii=False, indent=2)
            
            return True
        except Exception as e:
            logger.error("Failed to save language preference: %s", e)
            return False
    
    def set_language(self, language):
        """Set current language"""
        if language in self.TRANSLATIONS:
            self._current_language = language
            return True
        return False
    
    def get_language(self):
        """Get current language"""
        return self._current_language
    
    def get_language_name(self, lang_code):
        """Get language display name"""
        names = {
            self.LANG_ZH_CN: "简体中文",
            self.LANG_ZH_TW: "繁體中文",
            self.LANG_EN_US: "English"
        }
        return names.get(lang_code, lang_code)
    
    def get_available_languages(self):
        """Get list of available languages"""
        return [
            (self.LANG_ZH_CN, self.get_language_name(self.LANG_ZH_CN)),
            (self.LANG_ZH_TW, self.get_language_name(self.LANG_ZH_TW)),
            (self.LANG_EN_US, self.get_language_name(self.LANG_EN_US))
        ]
    
    def t(self, key, default=None):
        """
        Translate a key to current language
        :param key: Translation key
        :param default: Default value if key not found
        :return: Translated string
        """
        lang_dict = self.TRANSLATIONS.get(self._current_language, {})
        return lang_dict.get(key, default if default else key)


# Global instance
_i18n_instance = None

def get_i18n():
    """Get global i18n instance"""
    global _i18n_instance
    if _i18n_instance is None:
        _i18n_instance = I18n()
    return _i18n_instance

def t(key, default=None):
    """Shortcut function for translation"""
    return get_i18n().t(key, default)

