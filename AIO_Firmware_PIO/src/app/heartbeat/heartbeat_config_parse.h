// Pure parser over the raw bytes of /heartbeat_v2.01.cfg, extracted out
// of heartbeat.cpp::read_config so the snprintf-bounds invariant from
// PR-1.2 (Phase 1B) can be unit-tested without dragging PubSubClient,
// AppController, FlashFS, etc. into the test binary.
//
// The output struct mirrors only the persisted fields of
// HeartbeatAppForeverData (server / port / user / password / role /
// qq_num); derived runtime fields (mqtt_client_id, mac_id,
// mqtt_subtopic, mqtt_pubtopic, espClient, mqtt_client) are computed by
// the caller from the device MAC + parsed qq_num and are not in scope
// here.
//
// Field caps match HeartbeatAppForeverData exactly so the caller can
// memcpy field-for-field without any further bounds work.

#ifndef AIO_HEARTBEAT_CONFIG_PARSE_H
#define AIO_HEARTBEAT_CONFIG_PARSE_H

#include <stddef.h>
#include <stdint.h>

struct HeartbeatRawFields
{
    char mqtt_server[32];
    char mqtt_user[16];
    char mqtt_password[16];
    char qq_num[20];
    uint16_t mqtt_port;
    int role;
};

// Parse a NUL-terminated raw config buffer of length `size` (i.e. the
// bytes returned by g_flashCfg.readFile for /heartbeat_v2.01.cfg) into
// `out`. The buffer is mutated in place by analyseParam (each '\n'
// becomes '\0').
//
// Returns true on a non-empty buffer that contained the expected six
// fields; false on size == 0 (caller should seed defaults). The 32 /
// 16 / 16 / 20 byte caps inside `out` are enforced via snprintf — an
// oversize source is silently truncated and NUL-terminated.
//
// Behavior contract (matches the original heartbeat.cpp::read_config
// implementation): the input must contain at least 6 newlines; with
// fewer, analyseParam walks past the buffer end (UB inherited from the
// shared parser). Production write_config always emits exactly 6
// newlines.
bool heartbeat_parse_config(char *buffer_mut, size_t size, HeartbeatRawFields *out);

#endif
