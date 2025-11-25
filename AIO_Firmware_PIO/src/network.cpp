#include "network.h"
#include "common.h"
#include <esp_wifi.h>
#include "HardwareSerial.h"

IPAddress local_ip(192, 168, 4, 2); // Set your server's fixed IP address here
IPAddress gateway(192, 168, 4, 2);  // Set your network Gateway usually your Router base address
IPAddress subnet(255, 255, 255, 0); // Set your network sub-network mask here
IPAddress dns(192, 168, 4, 1);      // Set your network DNS usually your Router base address

const char *AP_SSID = "HoloCubic_AIO"; // 热点名称
const char *HOST_NAME = "HoloCubic";   // 主机名

uint16_t ap_timeout = 0; // ap无连接的超时时间

TimerHandle_t xTimer_ap;

Network::Network()
{
    m_preDisWifiConnInfoMillis = 0;
    m_wifiConnStartMillis = 0;
    m_isConnecting = false;
    WiFi.enableSTA(false);
    WiFi.enableAP(false);
}

void Network::search_wifi(void)
{
    Serial.println("scan start");
    int wifi_num = WiFi.scanNetworks();
    Serial.println("scan done");
    if (0 == wifi_num)
    {
        Serial.println("no networks found");
    }
    else
    {
        Serial.print(wifi_num);
        Serial.println(" networks found");
        for (int cnt = 0; cnt < wifi_num; ++cnt)
        {
            Serial.print(cnt + 1);
            Serial.print(": ");
            Serial.print(WiFi.SSID(cnt));
            Serial.print(" (");
            Serial.print(WiFi.RSSI(cnt));
            Serial.print(")");
            Serial.println((WiFi.encryptionType(cnt) == WIFI_AUTH_OPEN) ? " " : "*");
        }
    }
}

boolean Network::start_conn_wifi(const char *ssid, const char *password)
{
    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println(F("\nWiFi is already connected.\n"));
        m_isConnecting = false;
        return false;
    }
    
    // Validate SSID is not empty
    if (ssid == NULL || strlen(ssid) == 0)
    {
        Serial.println(F("\n[WiFi Error] SSID is empty! Please configure WiFi via web interface."));
        Serial.println(F("[WiFi Info] Connect to AP 'HoloCubic_AIO' at 192.168.4.2 to configure."));
        m_isConnecting = false;
        return false;
    }
    
    Serial.println(F("\n========== WiFi Connection Attempt =========="));
    Serial.print(F("SSID: "));
    Serial.println(ssid);
    Serial.print(F("Password: "));
    Serial.println(password && strlen(password) > 0 ? "********" : "<empty>");
    Serial.println(F("============================================\n"));

    // Set to STA mode and connect to WiFi
    WiFi.enableSTA(true);
    // Disable power saving mode to improve WiFi performance (either API works)
    // WiFi.setSleep(false);
    // esp_wifi_set_ps(WIFI_PS_NONE);
    // Set hostname
    WiFi.setHostname(HOST_NAME);
    WiFi.begin(ssid, password);
    
    m_preDisWifiConnInfoMillis = GET_SYS_MILLIS();
    m_wifiConnStartMillis = GET_SYS_MILLIS();
    m_isConnecting = true;

    return true;
}

boolean Network::end_conn_wifi(void)
{
    wl_status_t status = WiFi.status();
    
    if (WL_CONNECTED != status)
    {
        // Check for connection timeout
        if (m_isConnecting && (GET_SYS_MILLIS() - m_wifiConnStartMillis) > (CONN_ERR_TIMEOUT * 1000))
        {
            if (doDelayMillisTime(10000, &m_preDisWifiConnInfoMillis, false))
            {
                Serial.println(F("\n========== WiFi Connection Failed =========="));
                Serial.print(F("Status: "));
                switch(status)
                {
                    case WL_NO_SSID_AVAIL:
                        Serial.println(F("SSID not found"));
                        break;
                    case WL_CONNECT_FAILED:
                        Serial.println(F("Connection failed (wrong password?)"));
                        break;
                    case WL_CONNECTION_LOST:
                        Serial.println(F("Connection lost"));
                        break;
                    case WL_DISCONNECTED:
                        Serial.println(F("Disconnected"));
                        break;
                    case WL_IDLE_STATUS:
                        Serial.println(F("Idle (still trying to connect)"));
                        break;
                    default:
                        Serial.printf("Unknown (%d)\n", status);
                        break;
                }
                Serial.print(F("Duration: "));
                Serial.print((GET_SYS_MILLIS() - m_wifiConnStartMillis) / 1000);
                Serial.println(F(" seconds"));
                Serial.println(F("Suggestion: Check SSID/password in web settings"));
                Serial.println(F("============================================\n"));
            }
            m_isConnecting = false;
            return CONN_TIMEOUT;
        }
        
        if (doDelayMillisTime(10000, &m_preDisWifiConnInfoMillis, false))
        {
            // Reduce frequent printing
            Serial.print(F("."));
        }
        return CONN_ERROR;
    }

    // Successfully connected
    if (m_isConnecting || doDelayMillisTime(10000, &m_preDisWifiConnInfoMillis, false))
    {
        // Reduce frequent printing
        Serial.println(F("\n========== WiFi Connected =========="));
        Serial.print(F("SSID: "));
        Serial.println(WiFi.SSID());
        Serial.print(F("IP address: "));
        Serial.println(WiFi.localIP());
        Serial.print(F("Signal strength: "));
        Serial.print(WiFi.RSSI());
        Serial.println(F(" dBm"));
        Serial.println(F("====================================\n"));
        m_isConnecting = false;
    }
    return CONN_SUCC;
}

boolean Network::close_wifi(void)
{
    if (WiFi.getMode() & WIFI_MODE_AP)
    {
        WiFi.enableAP(false);
        Serial.println(F("AP shutdown"));
    }

    if (!WiFi.disconnect())
    {
        return false;
    }
    WiFi.enableSTA(false);
    WiFi.mode(WIFI_MODE_NULL);
    m_isConnecting = false;
    // esp_wifi_set_inactive_time(ESP_IF_ETH, 10); // Set temporary sleep time
    // esp_wifi_get_ant(wifi_ant_config_t * config); // Get temporary sleep time
    // WiFi.setSleep(WIFI_PS_MIN_MODEM);
    // WiFi.onEvent();
    Serial.println(F("WiFi disconnected"));
    return true;
}

unsigned long Network::get_conn_duration(void)
{
    if (!m_isConnecting)
    {
        return 0;
    }
    return GET_SYS_MILLIS() - m_wifiConnStartMillis;
}

boolean Network::open_ap(const char *ap_ssid, const char *ap_password)
{
    WiFi.enableAP(true); // 配置为AP模式
    // 修改主机名
    WiFi.setHostname(HOST_NAME);
    // WiFi.begin();
    boolean result = WiFi.softAP(ap_ssid, ap_password); // 开启WIFI热点
    if (result)
    {
        WiFi.softAPConfig(local_ip, gateway, subnet);
        IPAddress myIP = WiFi.softAPIP();

        // 打印相关信息
        Serial.print(F("\nSoft-AP IP address = "));
        Serial.println(myIP);
        Serial.println(String("MAC address = ") + WiFi.softAPmacAddress().c_str());
        Serial.println(F("waiting ..."));
        ap_timeout = 300; // 开始计时
        // xTimer_ap = xTimerCreate("ap time out", 1000 / portTICK_PERIOD_MS, pdTRUE, (void *)0, restCallback);
        // xTimerStart(xTimer_ap, 0); //开启定时器
    }
    else
    {
        // 开启热点失败
        Serial.println(F("WiFiAP Failed"));
        return false;
        delay(1000);
        ESP.restart(); // 复位esp32
    }
    // 设置域名
    if (MDNS.begin(HOST_NAME))
    {
        Serial.println(F("MDNS responder started"));
    }
    return true;
}

void restCallback(TimerHandle_t xTimer)
{
    // 长时间不访问WIFI Config 将复位设备
    --ap_timeout;
    Serial.print(F("AP timeout: "));
    Serial.println(ap_timeout);
    if (ap_timeout < 1)
    {
        // todo
        WiFi.softAPdisconnect(true);
        // ESP.restart();
    }
}