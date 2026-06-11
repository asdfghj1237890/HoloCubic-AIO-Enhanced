// AccuWeather HTTP fetchers + JSON parsing.
//
// Split out of weather.cpp in PR-3.2a. Pure mechanical extraction —
// every function body here is byte-for-byte the same as the pre-split
// weather.cpp (only the file boundary moved). Shared state (cfg_data,
// run_data) reaches in through weather_internal.h.

#include <Arduino.h>
#include "ArduinoJson.h"
#include "aio_network.h"
#include "common.h"
#include "http_util.h"
#include "weather_internal.h"

// AccuWeather Icon Codes Mapping
// https://developer.accuweather.com/weather-icons
// Icon codes range from 1-44; mapping to local weather icons:
// 0=sunny, 1=cloudy, 2=rainy, 3=partly_cloudy, 4=hail, 5=fog,
// 6=dust, 7=thunder, 8=snowy
int mapAccuWeatherIcon(int iconCode) {
    if (iconCode >= 1 && iconCode <= 2) return 0;      // Sunny/Mostly Sunny
    if (iconCode >= 3 && iconCode <= 4) return 3;      // Partly Sunny/Cloudy
    if (iconCode >= 5 && iconCode <= 6) return 3;      // Hazy Sunshine/Mostly Cloudy
    if (iconCode >= 7 && iconCode <= 8) return 1;      // Cloudy/Dreary
    if (iconCode == 11) return 5;                       // Fog
    if (iconCode == 12) return 2;                       // Showers
    if (iconCode >= 13 && iconCode <= 14) return 3;    // Mostly Cloudy w/ Showers
    if (iconCode == 15) return 7;                       // Thunderstorms
    if (iconCode >= 16 && iconCode <= 17) return 7;    // Mostly Cloudy w/ T-Storms
    if (iconCode == 18) return 2;                       // Rain
    if (iconCode >= 19 && iconCode <= 21) return 8;    // Flurries/Snow
    if (iconCode == 22) return 8;                       // Snow
    if (iconCode >= 23 && iconCode <= 24) return 8;    // Mostly Cloudy w/ Snow
    if (iconCode == 25) return 8;                       // Sleet
    if (iconCode == 26) return 8;                       // Freezing Rain
    if (iconCode >= 29 && iconCode <= 30) return 8;    // Rain and Snow
    if (iconCode >= 31 && iconCode <= 32) return 0;    // Hot/Cold
    if (iconCode >= 33 && iconCode <= 38) return 3;    // Night conditions (map to partly cloudy)
    if (iconCode >= 39 && iconCode <= 42) return 2;    // Night rain/showers
    if (iconCode >= 43 && iconCode <= 44) return 8;    // Night snow
    return 3;                                           // Default to partly cloudy
}

// Returns true once the user has supplied a real AccuWeather key (i.e. the
// stored value is non-empty and not equal to the seeded placeholder).
bool weather_api_key_configured(void)
{
    return cfg_data.api_key.length() > 0
        && cfg_data.api_key != WEATHER_API_KEY_PLACEHOLDER;
}

// New function for AccuWeather: Get location key (auto-detect by IP or search by city name)
bool get_location_key(void)
{
    if (WL_CONNECTED != WiFi.status())
        return false;

    if (!weather_api_key_configured())
    {
        Serial.println("[Weather] API key not set; configure via web settings");
        return false;
    }

    // If location key is already cached, check if we need to refresh
    if (cfg_data.location_key.length() > 0)
    {
        // Force refresh if city name is empty (user wants to use IP detection)
        if (cfg_data.city_name.length() == 0)
        {
            Serial.println("[Info] City name is empty, forcing IP-based re-detection...");
            cfg_data.location_key = "";  // Clear cached location key
        }
        else
        {
            Serial.printf("[Info] Using cached location key: %s (City: %s)\n",
                          cfg_data.location_key.c_str(),
                          cfg_data.city_name.c_str());
            return true;
        }
    }

    Serial.println("[Info] No cached location key, fetching from API...");

    char api[256] = {0};
    bool useIPDetection = (cfg_data.city_name.length() == 0);
    if (useIPDetection)
    {
        snprintf(api, 256, LOCATION_IP_API, cfg_data.api_key.c_str());
        Serial.println("[Info] Using IP-based location detection");
    }
    else
    {
        snprintf(api, 256, LOCATION_SEARCH_API,
                 cfg_data.api_key.c_str(),
                 cfg_data.city_name.c_str());
        Serial.printf("[Info] Searching for city: %s\n", cfg_data.city_name.c_str());
    }
    Serial.print("Location API = ");
    Serial.println(api);

    JsonDocument doc;
    int httpCode = 0;
    // ESP32-Weather-Station UA was set on this site by the original code
    // (PR-2.2a dropped it because the helper had no header support yet);
    // restored via the optional-header param added in PR-2.4.
    bool ok = http_fetch_json(api, doc, 3000, &httpCode,
                              "User-Agent", "ESP32-Weather-Station");
    Serial.printf("[HTTP] Response code: %d\n", httpCode);
    if (!ok)
    {
        if (httpCode <= 0)
        {
            Serial.printf("[HTTP] GET failed (code=%d)\n", httpCode);
        }
        else if (httpCode != HTTP_CODE_OK && httpCode != HTTP_CODE_MOVED_PERMANENTLY)
        {
            Serial.printf("[HTTP] Unexpected status code: %d\n", httpCode);
        }
        else
        {
            Serial.println("[JSON] Parse error");
            Serial.println("[Info] Try checking free heap or simplifying query");
        }
        return false;
    }

    // API error envelope
    if (doc["Code"].is<const char *>())
    {
        String errorCode = doc["Code"].as<String>();
        String errorMsg = doc["Message"].as<String>();
        Serial.printf("[API Error] Code: %s, Message: %s\n", errorCode.c_str(), errorMsg.c_str());
        return false;
    }

    // IP API returns single object, search API returns an array of candidates.
    JsonObject location;
    if (doc.is<JsonObject>())
    {
        location = doc.as<JsonObject>();
    }
    else if (doc.is<JsonArray>() && doc.size() > 0)
    {
        location = doc[0].as<JsonObject>();
    }

    if (!location.isNull() && location["Key"].is<const char *>())
    {
        cfg_data.location_key = location["Key"].as<String>();
        String cityName = location["LocalizedName"].as<String>();
        String country = location["Country"]["LocalizedName"].as<String>();
        if (useIPDetection || cfg_data.city_name.length() == 0)
        {
            cfg_data.city_name = location["EnglishName"].as<String>();
        }
        Serial.print("[Success] Location Key: ");
        Serial.println(cfg_data.location_key);
        Serial.printf("[Info] City: %s, Country: %s\n", cityName.c_str(), country.c_str());
        weather_write_config(&cfg_data);
        return true;
    }

    Serial.println("[APP] Get location key failed - no valid location data in response");

    // If city search failed, try IP-based detection as fallback (single retry).
    if (!useIPDetection && cfg_data.city_name.length() > 0)
    {
        Serial.println("[Info] City search failed, trying IP-based detection as fallback...");
        snprintf(api, 256, LOCATION_IP_API, cfg_data.api_key.c_str());
        Serial.print("Location API (IP fallback) = ");
        Serial.println(api);

        JsonDocument doc2;
        int httpCode2 = 0;
        // Same UA restore as the primary fetch above.
        if (http_fetch_json(api, doc2, 3000, &httpCode2,
                            "User-Agent", "ESP32-Weather-Station") && doc2.is<JsonObject>())
        {
                JsonObject location2 = doc2.as<JsonObject>();
            if (!location2.isNull() && location2["Key"].is<const char *>())
            {
                cfg_data.location_key = location2["Key"].as<String>();
                String cityName = location2["LocalizedName"].as<String>();
                String country = location2["Country"]["LocalizedName"].as<String>();
                cfg_data.city_name = location2["EnglishName"].as<String>();
                Serial.print("[Success] Location Key (IP fallback): ");
                Serial.println(cfg_data.location_key);
                Serial.printf("[Info] Auto-detected City: %s, Country: %s\n", cityName.c_str(), country.c_str());
                weather_write_config(&cfg_data);
                return true;
            }
        }
        Serial.println("[Error] IP-based fallback also failed");
    }

    if (useIPDetection)
    {
        Serial.println("[Info] IP-based detection failed");
        Serial.println("[Note] AccuWeather uses your public IP (WAN IP), not local 192.168.x.x");
        Serial.println("[Note] Your API key might be restricted to specific countries/regions");
    }

    Serial.println("[Suggestion] This API key might only work with specific cities");
    Serial.println("[Examples] Try: Beijing, Shanghai, Guangzhou, Shenzhen, Chengdu, Wuhan");
    return false;
}

// New get_weather function for AccuWeather
void get_weather(void)
{
    if (WL_CONNECTED != WiFi.status())
        return;

    // Ensure we have location key first
    if (!get_location_key())
    {
        Serial.println("[APP] Cannot get weather - location key not available");
        return;
    }

    char api[256] = {0};
    snprintf(api, 256, WEATHER_CURRENT_API,
             cfg_data.location_key.c_str(),
             cfg_data.api_key.c_str());
    Serial.print("Current Weather API = ");
    Serial.println(api);

    JsonDocument doc;
    int httpCode = 0;
    bool ok = http_fetch_json(api, doc, 2000, &httpCode);
    if (!ok)
    {
        if (httpCode <= 0)
        {
            Serial.printf("[HTTP] GET... failed (code=%d)\n", httpCode);
        }
        else if (httpCode != HTTP_CODE_OK && httpCode != HTTP_CODE_MOVED_PERMANENTLY)
        {
            Serial.printf("[HTTP] Unexpected status code: %d\n", httpCode);
        }
        else
        {
            Serial.println("[APP] Get weather - JSON parse error");
        }
    }
    else
    {
        if (doc.is<JsonArray>() && doc.size() > 0)
        {
                /*
                AccuWeather Current Conditions Response Example:
                [{
                    "LocalObservationDateTime": "2024-03-21T18:30:00+08:00",
                    "WeatherText": "晴朗",
                    "WeatherIcon": 1,
                    "Temperature": {
                        "Metric": {"Value": 19.0, "Unit": "C"}
                    },
                    "RelativeHumidity": 38,
                    "Wind": {
                        "Direction": {"Localized": "東北"},
                        "Speed": {"Metric": {"Value": 5.5, "Unit": "km/h"}}
                    },
                    "UVIndex": 3,
                    "UVIndexText": "Moderate"
                }]
                */
                JsonObject current = doc[0].as<JsonObject>();

                // Each field uses ArduinoJson's `| fallback` operator so a
                // missing or wrong-type value yields the sentinel instead of
                // crashing on undefined .as<T>() conversion. Nested keys
                // (Temperature.Metric.Value, Wind.Direction.Localized, etc) are
                // safe to chain because intermediate misses propagate as null
                // variants until the leaf operator| picks the fallback.

                // City name (use configured city name)
                snprintf(run_data->wea.cityname, sizeof(run_data->wea.cityname), "%s", cfg_data.city_name.c_str());

                // Temperature (Celsius)
                run_data->wea.temperature = current["Temperature"]["Metric"]["Value"] | 0;

                // Humidity
                run_data->wea.humidity = current["RelativeHumidity"] | 0;

                // Weather icon code mapping
                int iconCode = current["WeatherIcon"] | 0;
                run_data->wea.weather_code = mapAccuWeatherIcon(iconCode);

                // Weather description
                const char *weatherText = current["WeatherText"] | "";
                snprintf(run_data->wea.weather, sizeof(run_data->wea.weather), "%s", weatherText);

                // Wind direction
                const char *windDir = current["Wind"]["Direction"]["Localized"] | "";
                snprintf(run_data->wea.windDir, sizeof(run_data->wea.windDir), "%s", windDir);

                // Wind speed (convert km/h to level)
                float windSpeed = current["Wind"]["Speed"]["Metric"]["Value"] | 0.0f;
                int windLevel = (int)(windSpeed / 5.0); // Rough conversion: 0-5km/h = level 0, 5-10 = level 1, etc.
                // Symmetric clamp: a malformed/negative JSON value would
                // otherwise reach snprintf as e.g. "-2147483648" (11 chars
                // + NUL) and overflow windpower[10]. The >12 cap was here
                // pre-split; <0 added so gcc's range analysis can prove
                // the snprintf fits.
                if (windLevel > 12) windLevel = 12;
                if (windLevel < 0)  windLevel = 0;
                snprintf(run_data->wea.windpower, sizeof(run_data->wea.windpower), "%d", windLevel);

                // Air quality estimate from UV index (since AccuWeather doesn't provide AQI in free tier)
                int uvIndex = current["UVIndex"] | 0;
                if (uvIndex <= 2) run_data->wea.airQulity = 0;      // Good
                else if (uvIndex <= 5) run_data->wea.airQulity = 1; // Moderate
                else if (uvIndex <= 7) run_data->wea.airQulity = 2; // Fair
                else if (uvIndex <= 10) run_data->wea.airQulity = 3; // Poor
                else run_data->wea.airQulity = 4;                    // Very Poor

                Serial.println("Get AccuWeather current conditions OK\n");
            }
            else
            {
                Serial.println("[APP] Get weather error - invalid response format");
            }
    }
}

// New get_daliyWeather function for AccuWeather
void get_daliyWeather(short maxT[], short minT[])
{
    if (WL_CONNECTED != WiFi.status())
        return;

    // Ensure we have location key first
    if (!get_location_key())
    {
        Serial.println("[APP] Cannot get forecast - location key not available");
        return;
    }

    char api[256] = {0};
    snprintf(api, 256, WEATHER_FORECAST_API,
             cfg_data.location_key.c_str(),
             cfg_data.api_key.c_str());
    Serial.print("Forecast API = ");
    Serial.println(api);

    JsonDocument doc2;
    int httpCode = 0;
    bool ok = http_fetch_json(api, doc2, 2000, &httpCode);
    Serial.printf("[HTTP] Forecast response code: %d\n", httpCode);

    if (!ok)
    {
        if (httpCode <= 0)
        {
            Serial.printf("[HTTP] GET... failed (code=%d)\n", httpCode);
        }
        else if (httpCode != HTTP_CODE_OK && httpCode != HTTP_CODE_MOVED_PERMANENTLY)
        {
            Serial.printf("[HTTP] Unexpected status code: %d\n", httpCode);
        }
        else
        {
            // Helper succeeded HTTP fetch but JSON parse failed (or body
            // was empty: deserializeJson treats empty input as InvalidInput).
            Serial.println("[JSON] Forecast parse error or empty body");
            Serial.println("[Info] Forecast feature disabled, but current weather works fine");
        }
        return;
    }

    // Check for API error envelope
    if (doc2["Code"].is<const char *>())
    {
        String errorCode = doc2["Code"].as<String>();
        String errorMsg = doc2["Message"].as<String>();
        Serial.printf("[API Error] Forecast - Code: %s, Message: %s\n", errorCode.c_str(), errorMsg.c_str());
        return;
    }

    if (doc2["DailyForecasts"].is<JsonArray>())
    {
                /*
                AccuWeather 5-Day Forecast Response Example:
                {
                    "DailyForecasts": [
                        {
                            "Date": "2024-03-21T07:00:00+08:00",
                            "Temperature": {
                                "Minimum": {"Value": 10.0, "Unit": "C"},
                                "Maximum": {"Value": 25.0, "Unit": "C"}
                            },
                            "Day": {"Icon": 3, "IconPhrase": "Partly Sunny"},
                            "Night": {"Icon": 35, "IconPhrase": "Partly Cloudy"}
                        },
                        ...
                    ]
                }
                */
                JsonArray forecasts = doc2["DailyForecasts"].as<JsonArray>();
                int numDays = min((int)forecasts.size(), FORECAST_DAYS);

                for (int i = 0; i < numDays; i++)
                {
                    JsonObject day = forecasts[i].as<JsonObject>();
                    maxT[i] = day["Temperature"]["Maximum"]["Value"] | 0;
                    minT[i] = day["Temperature"]["Minimum"]["Value"] | 0;
                }

                Serial.println("Get AccuWeather forecast OK\n");
    }
    else
    {
        Serial.println("[APP] Get forecast error - invalid response format");
    }
}
