#ifndef AIO_STUB_SPIFFS_H
#define AIO_STUB_SPIFFS_H
#include "FS.h"
class SPIFFSClass : public fs::FS {
public:
    bool begin(bool = false, const char * = "/spiffs", uint8_t = 10) { return true; }
    void end() {}
    size_t totalBytes() { return 1024 * 1024; }
    size_t usedBytes() { return 0; }
};
extern SPIFFSClass SPIFFS;
#endif
