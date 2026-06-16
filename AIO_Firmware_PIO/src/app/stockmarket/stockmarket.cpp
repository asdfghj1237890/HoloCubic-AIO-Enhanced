#include "stockmarket.h"
#include "stockmarket_config_parse.h"
#include "stockmarket_color_rule.h"
#include "stockmarket_refresh_scheduler.h"
#include "stockmarket_yahoo_price.h"
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
#define STOCK_REFRESH_EVENT_TIMEOUT_MS (30000UL)

// STOCKmarket configuration for persistence
#define B_CONFIG_PATH "/stockmarket.cfg"
struct B_Config
{
    String stock_symbol;          // Stock symbol (e.g., AAPL, TSLA, 601126)
    String market_type;           // Market type: CN (China), US (USA), HK (Hong Kong), etc.
    unsigned long updataInterval; // Update interval (milliseconds)
    StockmarketColorRule color_rule; // Screen and LED up/down color convention
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
    w_data = w_data + stockmarket_color_rule_to_string(cfg->color_rule) + "\n";
    g_flashCfg.writeFile(B_CONFIG_PATH, w_data.c_str());
}

static void read_config(B_Config *cfg)
{
    // Read persistent configuration from flash
    // Config filename should start with APP name and end with ".cfg" to avoid conflicts
    char info[128] = {0};
    uint16_t size = g_flashCfg.readFile(B_CONFIG_PATH, (uint8_t *)info, sizeof(info));
    bool truncated = size >= sizeof(info) - 1;

    StockmarketRawConfig raw_cfg;
    bool valid = stockmarket_parse_config(info, size, truncated, &raw_cfg);
    cfg->stock_symbol = raw_cfg.stock_symbol;
    cfg->market_type = raw_cfg.market_type;
    cfg->updataInterval = raw_cfg.updataInterval;
    if (!stockmarket_color_rule_from_string(raw_cfg.color_rule, &cfg->color_rule))
    {
        cfg->color_rule = STOCKMARKET_COLOR_RULE_UP_GREEN;
    }

    if (!valid)
    {
        Serial.println("[Stock] Invalid config, rewriting defaults");
        write_config(cfg);
    }
}

struct StockmarketAppRunData
{
    unsigned int refresh_status;
    StockmarketRefreshScheduler refresh_scheduler;
    bool market_open;
    bool yahoo_meta_valid;
    long stock_utc_epoch;
    StockmarketYahooMeta yahoo_meta;
    StockmarketYahooSession yahoo_session;
    char sina_quote_date[11];
    char sina_quote_time[9];
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

static void stockmarket_reset_yahoo_meta()
{
    if (NULL == run_data)
    {
        return;
    }

    memset(&run_data->yahoo_meta, 0, sizeof(run_data->yahoo_meta));
    run_data->yahoo_session = STOCKMARKET_YAHOO_SESSION_CLOSED;
    run_data->yahoo_meta_valid = false;
}

static void stockmarket_apply_led()
{
    if (NULL == run_data)
    {
        return;
    }

    const bool is_up = (run_data->stockdata.updownflag == 1);
    StockmarketRgbColor color =
        stockmarket_led_color(cfg_data.color_rule, run_data->market_open, is_up);

    rgb_stop();
    if (color.on)
    {
        rgb.setBrightness(0.20f).setRGB(color.r, color.g, color.b);
    }
    else
    {
        rgb.setRGB(0, 0, 0);
    }
}

static bool stockmarket_is_cn_regular_session()
{
    if (NULL == run_data || '\0' == run_data->sina_quote_date[0])
    {
        return false;
    }

    String today = rtc.getTime(String("%Y-%m-%d"));
    if (today != run_data->sina_quote_date)
    {
        return false;
    }

    int day_of_week = rtc.getDayofWeek();
    if (0 == day_of_week || 6 == day_of_week)
    {
        return false;
    }

    int minutes = rtc.getHour(true) * 60 + rtc.getMinute();
    return (minutes >= (9 * 60 + 30) && minutes < (11 * 60 + 30)) ||
           (minutes >= (13 * 60) && minutes < (15 * 60));
}

static bool sina_copy_field(const String& payload,
                            int field_index,
                            char *out,
                            size_t out_len)
{
    if (NULL == out || 0 == out_len || field_index < 0)
    {
        return false;
    }
    out[0] = '\0';

    int start = payload.indexOf('"');
    if (start < 0)
    {
        return false;
    }
    ++start;

    for (int i = 0; i < field_index; ++i)
    {
        int comma = payload.indexOf(',', start);
        if (comma < 0)
        {
            return false;
        }
        start = comma + 1;
    }

    int end = payload.indexOf(',', start);
    int quote_end = payload.indexOf('"', start);
    if (end < 0 || (quote_end >= 0 && quote_end < end))
    {
        end = quote_end;
    }
    if (end <= start)
    {
        return false;
    }

    snprintf(out, out_len, "%s", payload.substring(start, end).c_str());
    return true;
}

static bool stockmarket_format_sina_quote_datetime(char *out,
                                                   size_t out_len,
                                                   const char *date,
                                                   const char *time)
{
    if (NULL == out || 0 == out_len || NULL == date || NULL == time ||
        strlen(date) < 10 || strlen(time) < 5)
    {
        return false;
    }

    snprintf(out, out_len, "%c%c:%c%c",
             time[0], time[1], time[3], time[4]);
    return true;
}

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
    unsigned long started = GET_SYS_MILLIS();

    if (market == "CN")
    {
        // Chinese market: Sina Finance API. The `referer` header is
        // load-bearing — Sina returns 403 / empty body without it.
        url = "http://hq.sinajs.cn/list=" + stockSymbol;
        Serial.printf("[StockHTTP] quote start market=%s symbol=%s\n",
                      market.c_str(), stockSymbol.c_str());
        result.httpCode = http_fetch_string(url.c_str(), result.httpResponse, 2000,
                                            "referer", "https://finance.sina.com.cn");
    }
    else
    {
        // International markets: Yahoo Finance v8 chart API. Yahoo blocks
        // the default ESP32 User-Agent; "Mozilla/5.0" is the smallest UA
        // that gets a normal JSON body.
        url = "https://query1.finance.yahoo.com/v8/finance/chart/" + stockSymbol +
              "?interval=15m&range=1d&includePrePost=true";
        Serial.printf("[StockHTTP] quote start market=%s symbol=%s\n",
                      market.c_str(), stockSymbol.c_str());
        result.httpCode = http_fetch_string(url.c_str(), result.httpResponse, 4000,
                                            "User-Agent", "Mozilla/5.0");
    }

    Serial.printf("[StockHTTP] quote done code=%d ms=%lu bytes=%u\n",
                  result.httpCode,
                  GET_SYS_MILLIS() - started,
                  (unsigned int)result.httpResponse.length());
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
    run_data->stockdata.color_rule = cfg_data.color_rule;
    run_data->stockdata.symbol[0]  = '\0';
    run_data->stockdata.company[0] = '\0';
    run_data->stockdata.datetime_str[0] = '\0';
    run_data->refresh_status = 0;
    run_data->stockdata.tradvolume = 0;
    run_data->stockdata.turnover = 0;
    run_data->market_open = false;
    run_data->stock_utc_epoch = 0;
    stockmarket_reset_yahoo_meta();
    run_data->sina_quote_date[0] = '\0';
    run_data->sina_quote_time[0] = '\0';
    stockmarket_refresh_scheduler_init(&run_data->refresh_scheduler,
                                       GET_SYS_MILLIS());

    display_stockmarket(run_data->stockdata, LV_SCR_LOAD_ANIM_NONE);
    stockmarket_apply_led();
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

    unsigned long now = GET_SYS_MILLIS();
    stockmarket_refresh_scheduler_expire_in_flight(
        &run_data->refresh_scheduler,
        now,
        STOCK_REFRESH_EVENT_TIMEOUT_MS);

    if (stockmarket_refresh_scheduler_begin_if_due(&run_data->refresh_scheduler, now))
    {
        int rc = sys->send_to(STOCK_APP_NAME, CTRL_NAME,
                              APP_MESSAGE_WIFI_CONN, NULL, NULL);
        if (0 != rc)
        {
            stockmarket_refresh_scheduler_finish(&run_data->refresh_scheduler,
                                                now,
                                                cfg_data.updataInterval);
        }
    }
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
    if (!sina_copy_field(payload, 30, run_data->sina_quote_date,
                         sizeof(run_data->sina_quote_date)) ||
        !sina_copy_field(payload, 31, run_data->sina_quote_time,
                         sizeof(run_data->sina_quote_time)))
    {
        Serial.println("[Stock] Sina payload: missing quote date/time — abort parse");
        return false;
    }

    return true;
}

// Parse international market data from Yahoo Finance API
static bool parse_yahoo_data(const String& payload)
{
    Serial.println("[HTTP] Parsing Yahoo Finance data");
    
    JsonDocument doc;
    
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error)
    {
        Serial.print("[JSON] Parse failed: ");
        Serial.println(error.c_str());
        return false;
    }
    
    JsonObject chart = doc["chart"]["result"][0].as<JsonObject>();
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
    // ArduinoJson `|` returns the right operand on missing key OR explicit null.
    const char* yahoo_short = chart["meta"]["shortName"] | (const char*)nullptr;
    const char* yahoo_long  = chart["meta"]["longName"]  | (const char*)nullptr;
    const char* yahoo_company = yahoo_short ? yahoo_short
                              : yahoo_long  ? yahoo_long
                              : yahoo_symbol;
    snprintf(run_data->stockdata.company, sizeof(run_data->stockdata.company),
             "%s", yahoo_company);
    
    // Get regular price, previous close, and extended-hours session metadata.
    JsonObject meta = chart["meta"].as<JsonObject>();
    float currentPrice = 0;
    float previousClose = 0;
    stockmarket_reset_yahoo_meta();
    run_data->market_open = false;
    run_data->sina_quote_date[0] = '\0';
    run_data->sina_quote_time[0] = '\0';
    
    // Get current price
    if (!meta["regularMarketPrice"].isNull()) {
        currentPrice = meta["regularMarketPrice"].as<float>();
    }
    
    // Get previous close - try chartPreviousClose first, then previousClose
    if (!meta["chartPreviousClose"].isNull()) {
        previousClose = meta["chartPreviousClose"].as<float>();
    } else if (!meta["previousClose"].isNull()) {
        previousClose = meta["previousClose"].as<float>();
    }

    run_data->yahoo_meta.regular_price = currentPrice;
    run_data->yahoo_meta.previous_close = previousClose;
    run_data->yahoo_meta.regular_market_time = meta["regularMarketTime"] | 0L;

    JsonObject periods = meta["currentTradingPeriod"].as<JsonObject>();
    if (!periods.isNull())
    {
        JsonObject pre = periods["pre"].as<JsonObject>();
        JsonObject regular = periods["regular"].as<JsonObject>();
        JsonObject post = periods["post"].as<JsonObject>();
        run_data->yahoo_meta.pre_start = pre["start"] | 0L;
        run_data->yahoo_meta.pre_end = pre["end"] | 0L;
        run_data->yahoo_meta.regular_start = regular["start"] | 0L;
        run_data->yahoo_meta.regular_end = regular["end"] | 0L;
        run_data->yahoo_meta.post_start = post["start"] | 0L;
        run_data->yahoo_meta.post_end = post["end"] | 0L;
    }
    run_data->yahoo_meta.gmtoffset = meta["gmtoffset"] | 0L;
    
    // Get OHLC data from quotes
    JsonArray timestamps = chart["timestamp"].as<JsonArray>();
    JsonArray close = chart["indicators"]["quote"][0]["close"].as<JsonArray>();
    JsonArray high = chart["indicators"]["quote"][0]["high"].as<JsonArray>();
    JsonArray low = chart["indicators"]["quote"][0]["low"].as<JsonArray>();
    JsonArray open = chart["indicators"]["quote"][0]["open"].as<JsonArray>();
    JsonArray volume = chart["indicators"]["quote"][0]["volume"].as<JsonArray>();

    if (!timestamps.isNull() && !close.isNull())
    {
        size_t item_count = timestamps.size();
        if (close.size() < item_count)
        {
            item_count = close.size();
        }

        for (int i = (int)item_count - 1; i >= 0; --i)
        {
            if (!timestamps[i].isNull() && !close[i].isNull())
            {
                run_data->yahoo_meta.latest_timestamp = timestamps[i].as<long>();
                run_data->yahoo_meta.latest_price = close[i].as<float>();
                break;
            }
        }
    }
    
    run_data->stockdata.NowQuo = currentPrice;
    run_data->stockdata.CloseQuo = previousClose;
    run_data->yahoo_meta_valid = true;
    
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
    if (NULL != run_data)
    {
        run_data->market_open = false;
        run_data->stock_utc_epoch = 0;
        stockmarket_reset_yahoo_meta();
    }
    
    MyHttpResult result = http_request(cfg_data.stock_symbol, cfg_data.market_type);
    
    if (-1 == result.httpCode)
    {
        Serial.println("[HTTP] Http request failed.");
        stockmarket_apply_led();
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
                stockmarket_apply_led();
                return;
            }
            
            // Bootstrap the RTC from Taobao's timestamp endpoint — same
            // pattern weather + anniversary use. Cheap (1 sec HTTP GET) and
            // avoids needing a configTime/SNTP setup at firmware boot. If
            // the fetch fails we still get whatever the RTC currently holds
            // (boot epoch ~"01-01 00:00" on cold start), which is bad but
            // not crashy — and the next update_stock_data tick retries.
            String ts_payload;
            bool stock_time_updated = false;
            unsigned long time_started = GET_SYS_MILLIS();
            Serial.println("[StockHTTP] time start");
            int ts_code = http_fetch_string(STOCK_TIME_API, ts_payload, 1500);
            Serial.printf("[StockHTTP] time done code=%d ms=%lu bytes=%u\n",
                          ts_code,
                          GET_SYS_MILLIS() - time_started,
                          (unsigned int)ts_payload.length());
            if (ts_code == HTTP_CODE_OK)
            {
                int t_idx = ts_payload.indexOf("\"t\":\"");
                if (t_idx >= 0)
                {
                    t_idx += 5;
                    int t_end = ts_payload.indexOf("\"", t_idx);
                    if (t_end > t_idx)
                    {
                        long long utc_ms = atoll(ts_payload.substring(t_idx, t_end).c_str());
                        run_data->stock_utc_epoch = (long)(utc_ms / 1000);
                        long long ms = utc_ms + STOCK_TZ_OFFSET_MS;
                        rtc.setTime(ms / 1000, 0);
                        stock_time_updated = true;
                    }
                }
            }

            if (cfg_data.market_type == "CN")
            {
                run_data->market_open = stock_time_updated &&
                                        stockmarket_is_cn_regular_session();
            }
            else
            {
                long yahoo_quote_timestamp =
                    run_data->yahoo_meta.regular_market_time > run_data->yahoo_meta.latest_timestamp
                        ? run_data->yahoo_meta.regular_market_time
                        : run_data->yahoo_meta.latest_timestamp;

                if (run_data->yahoo_meta_valid)
                {
                    StockmarketYahooSelection selection =
                        stockmarket_yahoo_select_price(&run_data->yahoo_meta,
                                                       stock_time_updated
                                                           ? run_data->stock_utc_epoch
                                                           : 0L);
                    run_data->stockdata.NowQuo = selection.price;
                    run_data->market_open = stock_time_updated && selection.market_active;
                    run_data->yahoo_session = stock_time_updated
                                                  ? selection.session
                                                  : STOCKMARKET_YAHOO_SESSION_CLOSED;
                    yahoo_quote_timestamp = selection.quote_timestamp;
                }
                else
                {
                    run_data->market_open = false;
                    run_data->yahoo_session = STOCKMARKET_YAHOO_SESSION_CLOSED;
                }

                long yahoo_display_timestamp = stockmarket_yahoo_display_timestamp(
                    stock_time_updated ? run_data->stock_utc_epoch : 0L,
                    yahoo_quote_timestamp);
                if (!stockmarket_yahoo_format_exchange_datetime(
                        run_data->stockdata.datetime_str,
                        sizeof(run_data->stockdata.datetime_str),
                        yahoo_display_timestamp,
                        run_data->yahoo_meta.gmtoffset))
                {
                    run_data->stockdata.datetime_str[0] = '\0';
                }
            }

            if (cfg_data.market_type == "CN")
            {
                if (stock_time_updated)
                {
                    String datetime = rtc.getTime(String("%H:%M"));
                    snprintf(run_data->stockdata.datetime_str,
                             sizeof(run_data->stockdata.datetime_str),
                             "%s", datetime.c_str());
                }
                else if (!stockmarket_format_sina_quote_datetime(
                             run_data->stockdata.datetime_str,
                             sizeof(run_data->stockdata.datetime_str),
                             run_data->sina_quote_date,
                             run_data->sina_quote_time))
                {
                    run_data->stockdata.datetime_str[0] = '\0';
                }
            }

            if (cfg_data.market_type != "CN" &&
                run_data->stockdata.tradvolume > 0 &&
                run_data->stockdata.NowQuo > 0)
            {
                run_data->stockdata.turnover =
                    run_data->stockdata.NowQuo * run_data->stockdata.tradvolume;
            }

            // Calculate change values
            run_data->stockdata.ChgValue = run_data->stockdata.NowQuo - run_data->stockdata.CloseQuo;
            run_data->stockdata.ChgPercent = (run_data->stockdata.CloseQuo != 0) 
                ? (run_data->stockdata.ChgValue / run_data->stockdata.CloseQuo * 100) 
                : 0;
            
            // Set up/down flag
            run_data->stockdata.updownflag = (run_data->stockdata.ChgValue >= 0) ? 1 : 0;
            run_data->stockdata.color_rule = cfg_data.color_rule;

            if (cfg_data.market_type == "CN")
            {
                Serial.printf("[Stock] %s: %.2f (%.2f%%) time=%s\n",
                    run_data->stockdata.symbol,
                    run_data->stockdata.NowQuo,
                    run_data->stockdata.ChgPercent,
                    run_data->stockdata.datetime_str);
            }
            else
            {
                Serial.printf("[Stock] %s: %.2f (%.2f%%) [%s] time=%s\n",
                    run_data->stockdata.symbol,
                    run_data->stockdata.NowQuo,
                    run_data->stockdata.ChgPercent,
                    stockmarket_yahoo_session_to_string(run_data->yahoo_session),
                    run_data->stockdata.datetime_str);
            }
            stockmarket_apply_led();
        }
        else
        {
            Serial.println("[HTTP] Non-OK response");
            stockmarket_apply_led();
        }
    }
    else
    {
        Serial.println("[HTTP] ERROR");
        stockmarket_apply_led();
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
        stockmarket_refresh_scheduler_finish(&run_data->refresh_scheduler,
                                            GET_SYS_MILLIS(),
                                            cfg_data.updataInterval);
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
            snprintf((char *)ext_info, 32, "%lu", cfg_data.updataInterval);
        }
        else if (!strcmp(param_key, "color_rule"))
        {
            snprintf((char *)ext_info, 32, "%s",
                     stockmarket_color_rule_to_string(cfg_data.color_rule));
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
            unsigned long interval = 0;
            if (stockmarket_parse_interval(param_val, &interval))
            {
                cfg_data.updataInterval = interval;
            }
        }
        else if (!strcmp(param_key, "color_rule"))
        {
            StockmarketColorRule color_rule = STOCKMARKET_COLOR_RULE_UP_GREEN;
            if (stockmarket_color_rule_from_string(param_val, &color_rule))
            {
                cfg_data.color_rule = color_rule;
                if (NULL != run_data)
                {
                    run_data->stockdata.color_rule = cfg_data.color_rule;
                }
                stockmarket_apply_led();
            }
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
