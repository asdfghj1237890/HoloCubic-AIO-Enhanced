#ifndef FILE_MSG_H
#define FILE_MSG_H

#include "stdint.h"
#include <string.h>

enum MODULE_TYPE : unsigned char
{
    MODULE_TYPE_UNKNOW = 0,
    MODULE_TYPE_CUBIC_FILE_MANAGER,
    MODULE_TYPE_C_FILE_MANAGER,
    // CUBIC_SETTINGS / TOOL_SETTINGS removed with B15 fix PR3 — the serial
    // SettingsMsg flow was never functional and is superseded by HTTP
    // /api/settings + /save<Cat>Conf form handlers.
};

enum ACTION_TYPE : unsigned char
{
    AT_UNKNOWN = 0,
    AT_FREE_STATUS,

    AT_DIR_CREATE,
    AT_DIR_REMOVE,
    AT_DIR_RENAME,
    AT_DIR_LIST,

    AT_FILE_CREATE,
    AT_FILE_WRITE,
    AT_FILE_READ,
    AT_FILE_REMOVE,
    AT_FILE_RENAME,
    AT_FILE_GET_INFO,
    // AT_SETTING_SET / AT_SETTING_GET removed with B15 fix PR3 (see above).
};

class MsgHead
{
public:
    uint16_t m_header_mark;
    uint16_t m_msg_len;
    MODULE_TYPE m_from_who;
    MODULE_TYPE m_to_who;
    ACTION_TYPE m_action_type;

public:
    MsgHead(MODULE_TYPE from_who = MODULE_TYPE_UNKNOW,
            MODULE_TYPE to_who = MODULE_TYPE_UNKNOW);
    ~MsgHead(){};
    bool isLegal();
    uint32_t decode(const uint8_t *msg);
    uint32_t encode(uint8_t *msg);
};

// VALUE_TYPE enum + SettingsMsg class removed with B15 fix PR3. The serial
// settings protocol was never functional end-to-end (firmware decode/encode
// had bugs, prefs.* persistence calls were commented out). Studio's Settings
// tab now uses HTTP /api/settings; the egui tool's Settings tab still
// references the old types via aio-protocol::SettingMsg + settings_worker.rs
// and will be migrated in a follow-up PR.

class FileSystem
{
public:
    MsgHead m_msg_head;

public:
    FileSystem(ACTION_TYPE action_type = AT_FREE_STATUS);
    ~FileSystem(){};
    uint32_t decode(const uint8_t *msg);
    uint32_t encode(uint8_t *msg);
};

class DirCreate
{
public:
    FileSystem m_file_system;
    char m_dir_path[99];

public:
    DirCreate(const char *dir_name = NULL);
    ~DirCreate(){};
    uint32_t decode(const uint8_t *msg);
    uint32_t encode(uint8_t *msg);
};

class DirList
{
public:
    FileSystem m_file_system;
    char m_dir_path[99];
    char m_dir_info[400];   // 用来承载子文件的文件名 多个文件名之间使用\t分隔

public:
    DirList(const char *dir_path = NULL, const char *dir_info = NULL);
    ~DirList(){};
    uint32_t decode(const uint8_t *msg);
    uint32_t encode(uint8_t *msg);
};

#endif