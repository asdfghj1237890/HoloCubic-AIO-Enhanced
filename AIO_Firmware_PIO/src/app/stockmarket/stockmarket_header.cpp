#include "stockmarket_header.h"

#include <stdio.h>
#include <string.h>

static size_t utf8_codepoint_len(const char *text)
{
    const unsigned char c = (unsigned char)text[0];

    if (c < 0x80)
    {
        return 1;
    }

    size_t expected = 1;
    if ((c & 0xE0) == 0xC0)
    {
        expected = 2;
    }
    else if ((c & 0xF0) == 0xE0)
    {
        expected = 3;
    }
    else if ((c & 0xF8) == 0xF0)
    {
        expected = 4;
    }

    for (size_t i = 1; i < expected; ++i)
    {
        const unsigned char next = (unsigned char)text[i];
        if (next == '\0' || (next & 0xC0) != 0x80)
        {
            return 1;
        }
    }

    return expected;
}

static size_t codepoint_columns(const char *text)
{
    return ((unsigned char)text[0] < 0x80) ? 1 : 2;
}

size_t stockmarket_header_display_columns(const char *text)
{
    if (text == NULL)
    {
        return 0;
    }

    size_t columns = 0;
    for (size_t i = 0; text[i] != '\0';)
    {
        columns += codepoint_columns(&text[i]);
        i += utf8_codepoint_len(&text[i]);
    }

    return columns;
}

static bool append_limited(char *out,
                           size_t out_len,
                           size_t *pos,
                           size_t *columns,
                           const char *text,
                           size_t max_columns)
{
    if (text == NULL || out_len == 0)
    {
        return false;
    }

    bool truncated = false;
    for (size_t i = 0; text[i] != '\0';)
    {
        const size_t cp_len = utf8_codepoint_len(&text[i]);
        const size_t cp_cols = codepoint_columns(&text[i]);
        if ((*columns + cp_cols) > max_columns || (*pos + cp_len) >= out_len)
        {
            truncated = true;
            break;
        }

        memcpy(&out[*pos], &text[i], cp_len);
        *pos += cp_len;
        *columns += cp_cols;
        out[*pos] = '\0';
        i += cp_len;
    }

    return truncated;
}

static void trim_trailing_space(char *out, size_t *pos, size_t *columns)
{
    while (*pos > 0 && out[*pos - 1] == ' ')
    {
        --(*pos);
        --(*columns);
        out[*pos] = '\0';
    }
}

static void append_ellipsis(char *out, size_t out_len, size_t *pos, size_t *columns)
{
    while (*columns < STOCKMARKET_HEADER_MAX_COLUMNS && (*pos + 1) < out_len)
    {
        out[*pos] = '.';
        ++(*pos);
        ++(*columns);
        out[*pos] = '\0';
        if (*columns >= STOCKMARKET_HEADER_MAX_COLUMNS ||
            (*pos >= 3 && out[*pos - 1] == '.' && out[*pos - 2] == '.' && out[*pos - 3] == '.'))
        {
            break;
        }
    }
}

static void format_truncated(char *out,
                             size_t out_len,
                             const char *symbol,
                             const char *company)
{
    const size_t content_columns = STOCKMARKET_HEADER_MAX_COLUMNS - 3;
    const size_t content_out_len = (out_len > 4) ? (out_len - 3) : out_len;
    size_t pos = 0;
    size_t columns = 0;
    bool truncated = false;

    out[0] = '\0';
    truncated |= append_limited(out, content_out_len, &pos, &columns, symbol, content_columns);

    if (!truncated && company != NULL && company[0] != '\0')
    {
        truncated |= append_limited(out, content_out_len, &pos, &columns, " - ", content_columns);
        truncated |= append_limited(out, content_out_len, &pos, &columns, company, content_columns);
    }

    if (truncated)
    {
        trim_trailing_space(out, &pos, &columns);
        append_ellipsis(out, out_len, &pos, &columns);
    }
}

void stockmarket_format_header(char *out,
                               size_t out_len,
                               const char *symbol,
                               const char *company)
{
    if (out == NULL || out_len == 0)
    {
        return;
    }

    out[0] = '\0';
    if (symbol == NULL || symbol[0] == '\0')
    {
        snprintf(out, out_len, "--");
        return;
    }

    const bool has_company = (company != NULL && company[0] != '\0');
    const size_t symbol_columns = stockmarket_header_display_columns(symbol);
    const size_t company_columns = has_company
        ? stockmarket_header_display_columns(company)
        : 0;
    const size_t total_columns = symbol_columns + (has_company ? 3 + company_columns : 0);

    const size_t total_bytes = strlen(symbol) + (has_company ? 3 + strlen(company) : 0);
    if (total_columns <= STOCKMARKET_HEADER_MAX_COLUMNS && (total_bytes + 1) <= out_len)
    {
        if (has_company)
        {
            snprintf(out, out_len, "%s - %s", symbol, company);
        }
        else
        {
            snprintf(out, out_len, "%s", symbol);
        }
        if (stockmarket_header_display_columns(out) <= STOCKMARKET_HEADER_MAX_COLUMNS)
        {
            return;
        }
    }

    format_truncated(out, out_len, symbol, has_company ? company : NULL);
}
