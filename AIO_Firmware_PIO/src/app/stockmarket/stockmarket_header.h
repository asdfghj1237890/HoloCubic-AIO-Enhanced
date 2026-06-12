#ifndef AIO_STOCKMARKET_HEADER_H
#define AIO_STOCKMARKET_HEADER_H

#include <stddef.h>

#define STOCKMARKET_HEADER_MAX_COLUMNS 20u

#ifdef __cplusplus
extern "C"
{
#endif

size_t stockmarket_header_display_columns(const char *text);
void stockmarket_format_header(char *out,
                               size_t out_len,
                               const char *symbol,
                               const char *company);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
