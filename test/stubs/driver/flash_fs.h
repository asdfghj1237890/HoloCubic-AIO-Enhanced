// Guard matches firmware driver/flash_fs.h (added in this PR).
// See common.h header.
#ifndef FLASH_FS_H
#define FLASH_FS_H
#include "Arduino.h"
#include "FS.h"

class FlashFS {
public:
    FlashFS() {}
    ~FlashFS() {}
    void listDir(const char *, uint8_t) {}
    uint16_t readFile(const char *path, uint8_t *info);
    uint16_t readFile(const char *path, uint8_t *info, size_t info_len);
    void writeFile(const char *path, const char *message);
    void appendFile(const char *path, const char *message);
    void renameFile(const char *, const char *) {}
    void deleteFile(const char *path);
};

bool analyseParam(char *info, int argc, char **argv);

#endif
