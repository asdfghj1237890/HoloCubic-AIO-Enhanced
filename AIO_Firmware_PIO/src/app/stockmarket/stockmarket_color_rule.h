#ifndef AIO_STOCKMARKET_COLOR_RULE_H
#define AIO_STOCKMARKET_COLOR_RULE_H

#include <stdint.h>

#ifdef __cplusplus
#else
#include <stdbool.h>
#endif

#define STOCKMARKET_COLOR_RULE_UP_GREEN_TEXT "UP_GREEN"
#define STOCKMARKET_COLOR_RULE_UP_RED_TEXT "UP_RED"

typedef enum
{
    STOCKMARKET_COLOR_RULE_UP_GREEN = 0,
    STOCKMARKET_COLOR_RULE_UP_RED
} StockmarketColorRule;

typedef struct
{
    bool on;
    uint8_t r;
    uint8_t g;
    uint8_t b;
} StockmarketRgbColor;

#ifdef __cplusplus
extern "C" {
#endif

bool stockmarket_color_rule_from_string(const char *text,
                                        StockmarketColorRule *out);
const char *stockmarket_color_rule_to_string(StockmarketColorRule rule);
StockmarketRgbColor stockmarket_direction_color(StockmarketColorRule rule,
                                                bool is_up);
StockmarketRgbColor stockmarket_led_color(StockmarketColorRule rule,
                                          bool market_open,
                                          bool is_up);

#ifdef __cplusplus
}
#endif

#endif
