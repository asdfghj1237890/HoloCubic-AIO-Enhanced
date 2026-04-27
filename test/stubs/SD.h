#ifndef AIO_STUB_SD_H
#define AIO_STUB_SD_H
#include "FS.h"
class SDClass : public fs::FS {
public:
    bool begin(uint8_t = 0, uint32_t = 4000000) { return false; }
    void end() {}
    uint64_t cardSize() { return 0; }
    uint64_t totalBytes() { return 0; }
    uint64_t usedBytes() { return 0; }
};
extern SDClass SD;
#endif
