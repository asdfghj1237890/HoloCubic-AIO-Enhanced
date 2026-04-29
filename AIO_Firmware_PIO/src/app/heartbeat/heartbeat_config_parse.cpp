#include "heartbeat_config_parse.h"
#include "driver/flash_fs.h"  // analyseParam

#include <stdio.h>
#include <stdlib.h>

bool heartbeat_parse_config(char *buffer_mut, size_t size, HeartbeatRawFields *out)
{
    if (NULL == out) {
        return false;
    }
    if (0 == size) {
        return false;
    }

    char *param[6] = {0};
    analyseParam(buffer_mut, 6, param);

    snprintf(out->mqtt_server, sizeof(out->mqtt_server), "%s", param[0]);
    out->mqtt_port = (uint16_t)atol(param[1]);
    snprintf(out->mqtt_user, sizeof(out->mqtt_user), "%s", param[2]);
    snprintf(out->mqtt_password, sizeof(out->mqtt_password), "%s", param[3]);
    out->role = atoi(param[4]);
    snprintf(out->qq_num, sizeof(out->qq_num), "%s", param[5]);

    return true;
}
