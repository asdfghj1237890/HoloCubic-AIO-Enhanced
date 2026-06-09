#include "stockmarket.h"
#include "stockmarket_gui.h"
#include "sys/app_controller.h"
#include "../../common.h"
#include "../../http_util.h"
#include "ArduinoJson.h"
#include "ESP32Time.h"

// Taobao timestamp endpoint — same one weather/anniversary use to bootstrap
// the RTC without needing a configTime/SNTP setup. The +28800000 ms shifts
// the UTC epoch into UTC+8 (CST) which matches the firmware's default tz
// and what weather expects.
#define STOCK_TIME_API "https://acs.m.taobao.com/gw/mtop.common.getTimestamp/"
#define STOCK_TZ_OFFSET_MS (28800000LL)

// STOCKmarket configuration for persistence
#define B_CONFIG_PATH "/stockmarket.cfg"
struct B_Config
{
    String stock_symbol;          // Stock symbol (e.g., AAPL, TSLA, 601126)
    String market_type;           // Market type: CN (China), US (USA), HK (Hong Kong), etc.
    unsigned long updataInterval; // Update interval (milliseconds)
};

static void write_config(const B_Config *cfg)
{
    char tmp[16];
    // Save configuration data to file (persistence)
    String w_data;
    w_data = w_data + cfg->stock_symbol + "\n";
    w_data = w_data + cfg->market_type + "\n";
    memset(tmp, 0, 16);
    snprintf(tmp, 16, "%lu\n", cfg->updataInterval);
    w_data += tmp;
    g_flashCfg.writeFile(B_CONFIG_PATH, w_data.c_str());
}

static void read_config(B_Config *cfg)
{
    // Read persistent configuration from flash
    // Config filename should start with APP name and end with ".cfg" to avoid conflicts
    char info[128] = {0};
    uint16_t size = g_flashCfg.readFile(B_CONFIG_PATH, (uint8_t *)info);
    info[size] = 0;
    if (size == 0)
    {
        // Default values
        cfg->stock_symbol = "AAPL";    // Default: Apple Inc.
        cfg->market_type = "US";       // Default: US market
        cfg->updataInterval = 10000;   // Update interval: 10000ms (10s)
        write_config(cfg);
    }
    else
    {
        // Parse data
        char *param[3] = {0};
        analyseParam(info, 3, param);
        cfg->stock_symbol = param[0];
        cfg->market_type = param[1];
        cfg->updataInterval = atol(param[2]);
    }
}

struct StockmarketAppRunData
{
    unsigned int refresh_status;
    unsigned long refresh_time_millis;
    StockMarket stockdata;
};

struct MyHttpResult
{
    int httpCode = 0;
    String httpResponse = "";
};

static B_Config cfg_data;
static StockmarketAppRunData *run_data = NULL;
static ESP32Time rtc;

// Build stock symbol based on market type
static String buildStockSymbol(const String& symbol, const String& market)
{
    if (market == "CN")
    {
        // Chinese stocks: need sh/sz prefix for Sina API
        if (!symbol.startsWith("sh") && !symbol.startsWith("sz"))
        {
            // Default to Shanghai if no prefix
            return "sh" + symbol;
        }
        return symbol;
    }
    else if (market == "HK")
    {
        // Hong Kong stocks
        return symbol + ".HK";
    }
    else if (market == "US")
    {
        // US stocks: use symbol as-is
        return symbol;
    }
    return symbol;
}

static MyHttpResult http_request(const String& symbol, const String& market)
{
    MyHttpResult result;
    String url;
    String stockSymbol = buildStockSymbol(symbol, market);

    if (market == "CN")
    {
        // Chinese market: Sina Finance API. The `referer` header is
        // load-bearing — Sina returns 403 / empty body without it.
        url = "http://hq.sinajs.cn/list=" + stockSymbol;
        result.httpCode = http_fetch_string(url.c_str(), result.httpResponse, 2000,
                                            "referer", "https://finance.sina.com.cn");
    }
    else
    {
        // International markets: Yahoo Finance v8 chart API. Yahoo blocks
        // the default ESP32 User-Agent; "Mozilla/5.0" is the smallest UA
        // that gets a normal JSON body.
        url = "https://query1.finance.yahoo.com/v8/finance/chart/" + stockSymbol + "?interval=1d&range=1d";
        result.httpCode = http_fetch_string(url.c_str(), result.httpResponse, 3000,
                                            "User-Agent", "Mozilla/5.0");
    }

    return result;
}

static int stockmarket_init(AppController *sys)
{
    stockmarket_gui_init();
    // 获取配置信息
    read_config(&cfg_data);
    // 初始化运行时参数
    run_data = (StockmarketAppRunData *)malloc(sizeof(StockmarketAppRunData));
    run_data->stockdata.OpenQuo = 0;
    run_data->stockdata.CloseQuo = 0;
    run_data->stockdata.NowQuo = 0;
    run_data->stockdata.MaxQuo = 0;
    run_data->stockdata.MinQuo = 0;
    run_data->stockdata.ChgValue = 0;
    run_data->stockdata.ChgPercent = 0;
    run_data->stockdata.updownflag = 1;
    run_data->stockdata.symbol[0]  = '\0';
    run_data->stockdata.company[0] = '\0';
    run_data->stockdata.datetime_str[0] = '\0';
    run_data->refresh_status = 0;
    run_data->stockdata.tradvolume = 0;
    run_data->stockdata.turnover = 0;
    run_data->refresh_time_millis = GET_SYS_MILLIS() - cfg_data.updataInterval;

    display_stockmarket(run_data->stockdata, LV_SCR_LOAD_ANIM_NONE);
    return 0;
}

static void stockmarket_process(AppController *sys,
                                const ImuAction *act_info)
{
    lv_scr_load_anim_t anim_type = LV_SCR_LOAD_ANIM_FADE_ON;
    if (RETURN == act_info->active)
    {
        sys->send_to(STOCK_APP_NAME, CTRL_NAME,
                     APP_MESSAGE_WIFI_DISCONN, NULL, NULL);
        sys->app_exit(); // 退出APP
        return;
    }

    // 以下减少网络请求的压力
    if (doDelayMillisTime(cfg_data.updataInterval, &run_data->refresh_time_millis, false))
    {
        sys->send_to(STOCK_APP_NAME, CTRL_NAME,
                     APP_MESSAGE_WIFI_CONN, NULL, NULL);
    }

    delay(300);
}

static void stockmarket_background_task(AppController *sys,
                                        const ImuAction *act_info)
{
    // 本函数为后台任务，主控制器会间隔一分钟调用此函数
    // 本函数尽量只调用"常驻数据",其他变量可能会因为生命周期的缘故已经释放
}

static int stockmarket_exit_callback(void *param)
{
    stockmarket_gui_del();

    // 释放运行数据
    if (NULL != run_data)
    {
        free(run_data);
        run_data = NULL;
    }
    return 0;
}

// Parse Chinese market data from Sina Finance API.
// Expected payload shape:
//   var hq_str_sh603019="name,open,close,now,max,min,bid,ask,vol,turnover,...";
// Walk the comma-delimited fields with explicit -1 checks at every step
// so a truncated / non-Sina payload bails out instead of feeding negative
// indices into String::substring (which silently returns the whole tail
// and then atof's whatever junk comes back).
static bool parse_sina_data(const String& payload)
{
    Serial.println("[HTTP] Parsing Sina Finance data");

    // Stock name: between the first '"' and the first ','.
    int quote_pos = payload.indexOf('"');
    int first_comma = payload.indexOf(',');
    if (quote_pos < 0 || first_comma < 0 || first_comma <= quote_pos)
    {
        Serial.println("[Stock] Sina payload: missing quote/comma — abort parse");
        return false;
    }
    String Stockname = payload.substring(quote_pos + 1, first_comma);
    snprintf(run_data->stockdata.company, sizeof(run_data->stockdata.company),
             "%s", Stockname.c_str());
    snprintf(run_data->stockdata.symbol, sizeof(run_data->stockdata.symbol),
             "%s", cfg_data.stock_symbol.c_str());

    // Walk the next 9 fields. Sina's full payload has 30+ commas; we need
    // F1..F9 for the price metrics (F1..F5) plus volume (F8) + turnover (F9).
    float fields[9] = {0};
    int cursor = first_comma;
    for (int i = 0; i < 9; ++i)
    {
        int next = payload.indexOf(',', cursor + 1);
        if (next < 0)
        {
            Serial.printf("[Stock] Sina payload: ran out of commas at field %d — abort parse\n", i);
            return false;
        }
        fields[i] = payload.substring(cursor + 1, next).toFloat();
        cursor = next;
    }
    run_data->stockdata.OpenQuo  = fields[0];   // F1 open
    run_data->stockdata.CloseQuo = fields[1];   // F2 close
    run_data->stockdata.NowQuo   = fields[2];   // F3 now
    run_data->stockdata.MaxQuo   = fields[3];   // F4 max
    run_data->stockdata.MinQuo   = fields[4];   // F5 min
    // fields[5..6] = bid/ask (not displayed)
    run_data->stockdata.tradvolume = fields[7]; // F8 vol (corrected; was F9)
    run_data->stockdata.turnover   = fields[8]; // F9 turnover (corrected; was F10)

    return true;
}

// Parse international market data from Yahoo Finance API
static bool parse_yahoo_data(const String& payload)
{
    Serial.println("[HTTP] Parsing Yahoo Finance data");
    
    DynamicJsonDocument doc(4096);
    
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error)
    {
        Serial.print("[JSON] Parse failed: ");
        Serial.println(error.c_str());
        return false;
    }
    
    JsonObject chart = doc["chart"]["result"][0];
    if (chart.isNull())
    {
        Serial.println("[JSON] Invalid data structure");
        return false;
    }
    
    // Symbol: prefer meta.symbol, fallback to configured symbol
    const char* yahoo_symbol = chart["meta"]["symbol"] | cfg_data.stock_symbol.c_str();
    snprintf(run_data->stockdata.symbol, sizeof(run_data->stockdata.symbol),
             "%s", yahoo_symbol);

    // Company name: shortName → longName → symbol fallback chain.
    // ArduinoJson v6 `|` returns the right operand on missing key OR explicit null.
    const char* yahoo_short = chart["meta"]["shortName"] | (const char*)nullptr;
    const char* yahoo_long  = chart["meta"]["longName"]  | (const char*)nullptr;
    const char* yahoo_company = yahoo_short ? yahoo_short
                              : yahoo_long  ? yahoo_long
                              : yahoo_symbol;
    snprintf(run_data->stockdata.company, sizeof(run_data->stockdata.company),
             "%s", yahoo_company);
    
    // Get current price and previous close from meta
    JsonObject meta = chart["meta"];
    float currentPrice = 0;
    float previousClose = 0;
    
    // Get current price
    if (meta.containsKey("regularMarketPrice")) {
        currentPrice = meta["regularMarketPrice"];
    }
    
    // Get previous close - try chartPreviousClose first, then previousClose
    if (meta.containsKey("chartPreviousClose")) {
        previousClose = meta["chartPreviousClose"];
    } else if (meta.containsKey("previousClose")) {
        previousClose = meta["previousClose"];
    }
    
    // Get OHLC data from quotes
    JsonArray high = chart["indicators"]["quote"][0]["high"];
    JsonArray low = chart["indicators"]["quote"][0]["low"];
    JsonArray open = chart["indicators"]["quote"][0]["open"];
    JsonArray volume = chart["indicators"]["quote"][0]["volume"];
    
    run_data->stockdata.NowQuo = currentPrice;
    run_data->stockdata.CloseQuo = previousClose;
    
    // Get today's open, high, low (last valid value)
    if (open.size() > 0)
    {
        for (int i = open.size() - 1; i >= 0; i--)
        {
            if (!open[i].isNull())
            {
                run_data->stockdata.OpenQuo = open[i];
                break;
            }
        }
    }
    
    if (high.size() > 0)
    {
        for (int i = high.size() - 1; i >= 0; i--)
        {
            if (!high[i].isNull())
            {
                run_data->stockdata.MaxQuo = high[i];
                break;
            }
        }
    }
    
    if (low.size() > 0)
    {
        for (int i = low.size() - 1; i >= 0; i--)
        {
            if (!low[i].isNull())
            {
                run_data->stockdata.MinQuo = low[i];
                break;
            }
        }
    }
    
    if (volume.size() > 0)
    {
        for (int i = volume.size() - 1; i >= 0; i--)
        {
            if (!volume[i].isNull())
            {
                run_data->stockdata.tradvolume = volume[i];
                break;
            }
        }
    }
    
    // Calculate turnover: approximate as current price * volume
    // Yahoo Finance doesn't provide turnover directly, so we estimate it
    if (run_data->stockdata.tradvolume > 0 && currentPrice > 0) {
        run_data->stockdata.turnover = currentPrice * run_data->stockdata.tradvolume;
    } else {
        run_data->stockdata.turnover = 0;
    }
    
    return true;
}

static void update_stock_data()
{
    Serial.printf("[MEM] Free heap: %d bytes\n", ESP.getFreeHeap());
    
    MyHttpResult result = http_request(cfg_data.stock_symbol, cfg_data.market_type);
    
    if (-1 == result.httpCode)
    {
        Serial.println("[HTTP] Http request failed.");
        return;
    }
    
    if (result.httpCode > 0)
    {
        if (result.httpCode == HTTP_CODE_OK || result.httpCode == HTTP_CODE_MOVED_PERMANENTLY)
        {
            Serial.println("[HTTP] OK");
            
            bool parseSuccess = false;
            
            // Parse based on market type
            if (cfg_data.market_type == "CN")
            {
                parseSuccess = parse_sina_data(result.httpResponse);
            }
            else
            {
                parseSuccess = parse_yahoo_data(result.httpResponse);
            }
            
            if (!parseSuccess)
            {
                Serial.println("[Parse] Failed to parse stock data");
                return;
            }
            
            // Bootstrap the RTC from Taobao's timestamp endpoint — same
            // pattern weather + anniversary use. Cheap (1 sec HTTP GET) and
            // avoids needing a configTime/SNTP setup at firmware boot. If
            // the fetch fails we still get whatever the RTC currently holds
            // (boot epoch ~"01-01 00:00" on cold start), which is bad but
            // not crashy — and the next update_stock_data tick retries.
            String ts_payload;
            int ts_code = http_fetch_string(STOCK_TIME_API, ts_payload, 1500);
            if (ts_code == HTTP_CODE_OK)
            {
                int t_idx = ts_payload.indexOf("\"t\":\"");
                if (t_idx >= 0)
                {
                    t_idx += 5;
                    int t_end = ts_payload.indexOf("\"", t_idx);
                    if (t_end > t_idx)
                    {
                        long long ms = atoll(ts_payload.substring(t_idx, t_end).c_str())
                                       + STOCK_TZ_OFFSET_MS;
                        rtc.setTime(ms / 1000, 0);
                    }
                }
            }

            // Compact "MM-DD HH:MM". Must use getTime(String) — getDateTime
            // takes a `bool mode` (NOT a format string), so a const char*
            // implicitly converts to `true` and we get a long-form date
            // like "Thursday, January 1 1970 00:00:00" which then truncates
            // to "Thursday, J" in the buffer. getTime(String) is the right
            // strftime-format-string entry point.
            String datetime = rtc.getTime(String("%m-%d %H:%M"));
            snprintf(run_data->stockdata.datetime_str,
                     sizeof(run_data->stockdata.datetime_str),
                     "%s", datetime.c_str());

            // Calculate change values
            run_data->stockdata.ChgValue = run_data->stockdata.NowQuo - run_data->stockdata.CloseQuo;
            run_data->stockdata.ChgPercent = (run_data->stockdata.CloseQuo != 0) 
                ? (run_data->stockdata.ChgValue / run_data->stockdata.CloseQuo * 100) 
                : 0;
            
            // Set up/down flag
            run_data->stockdata.updownflag = (run_data->stockdata.ChgValue >= 0) ? 1 : 0;

            Serial.printf("[Stock] %s: %.2f (%.2f%%)\n",
                run_data->stockdata.symbol,
                run_data->stockdata.NowQuo,
                run_data->stockdata.ChgPercent);
        }
    }
    else
    {
        Serial.println("[HTTP] ERROR");
    }
}

static void stockmarket_message_handle(const char *from, const char *to,
                                       APP_MESSAGE_TYPE type, void *message,
                                       void *ext_info)
{
    switch (type)
    {
    case APP_MESSAGE_WIFI_CONN:
    {
        Serial.print(GET_SYS_MILLIS());
        Serial.println("[SYS] stockmarket_event_notification");
        update_stock_data();
        display_stockmarket(run_data->stockdata, LV_SCR_LOAD_ANIM_NONE);
    }
    break;
    case APP_MESSAGE_UPDATE_TIME:
    {
    }
    break;
    case APP_MESSAGE_GET_PARAM:
    {
        char *param_key = (char *)message;
        if (!strcmp(param_key, "stock_symbol"))
        {
            snprintf((char *)ext_info, 32, "%s", cfg_data.stock_symbol.c_str());
        }
        else if (!strcmp(param_key, "market_type"))
        {
            snprintf((char *)ext_info, 32, "%s", cfg_data.market_type.c_str());
        }
        else if (!strcmp(param_key, "updataInterval"))
        {
            snprintf((char *)ext_info, 32, "%u", cfg_data.updataInterval);
        }
        // Legacy support for old parameter name
        else if (!strcmp(param_key, "stock_id"))
        {
            snprintf((char *)ext_info, 32, "%s", cfg_data.stock_symbol.c_str());
        }
        else
        {
            snprintf((char *)ext_info, 32, "%s", "NULL");
        }
    }
    break;
    case APP_MESSAGE_SET_PARAM:
    {
        char *param_key = (char *)message;
        char *param_val = (char *)ext_info;
        if (!strcmp(param_key, "stock_symbol"))
        {
            cfg_data.stock_symbol = param_val;
        }
        else if (!strcmp(param_key, "market_type"))
        {
            cfg_data.market_type = param_val;
        }
        else if (!strcmp(param_key, "updataInterval"))
        {
            cfg_data.updataInterval = atol(param_val);
        }
        // Legacy support for old parameter name
        else if (!strcmp(param_key, "stock_id"))
        {
            cfg_data.stock_symbol = param_val;
        }
    }
    break;
    case APP_MESSAGE_READ_CFG:
    {
        read_config(&cfg_data);
    }
    break;
    case APP_MESSAGE_WRITE_CFG:
    {
        write_config(&cfg_data);
    }
    break;
    default:
        break;
    }
}

APP_OBJ stockmarket_app = {STOCK_APP_NAME, &app_stockmarket, "", stockmarket_init,
                           stockmarket_process, stockmarket_background_task,
                           stockmarket_exit_callback, stockmarket_message_handle};
