#ifndef APP_PC_RESOURCE_FMT_H
#define APP_PC_RESOURCE_FMT_H

#include <stdio.h>
#include <stddef.h>

// Net speeds arrive as KB/s scaled x10 (struct PC_Resource). The redesigned
// footer cells are fixed-width, so values >= 1000 KB/s promote to "M" instead
// of relying on the old scrolling labels.
static inline void pc_resource_format_speed(char *buf, size_t len, int raw_x10)
{
    if (raw_x10 < 0)
        raw_x10 = 0;
    int kbps = raw_x10 / 10;
    if (kbps < 1000)
        snprintf(buf, len, "%d.%dK", kbps, raw_x10 % 10);
    else
        snprintf(buf, len, "%d.%dM", kbps / 1000, (kbps % 1000) / 100);
}

#endif
