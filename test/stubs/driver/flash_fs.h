#ifndef AIO_STUB_DRIVER_FLASH_FS_H
#define AIO_STUB_DRIVER_FLASH_FS_H
#include "Arduino.h"
#include "FS.h"

class FlashFS {
public:
    FlashFS() {}
    ~FlashFS() {}
    void listDir(const char *, uint8_t) {}
    uint16_t readFile(const char *path, uint8_t *info);
    void writeFile(const char *path, const char *message);
    void appendFile(const char *path, const char *message);
    void renameFile(const char *, const char *) {}
    void deleteFile(const char *path);
};

bool analyseParam(char *info, int argc, char **argv);

#endif
