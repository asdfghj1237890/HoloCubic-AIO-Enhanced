// Guard matches firmware driver/sd_card.h. See common.h header.
#ifndef SD_CARD_H
#define SD_CARD_H
#include "FS.h"
#include "SD.h"
#include "SPI.h"

#define DIR_FILE_NUM 10
#define DIR_FILE_NAME_MAX_LEN 20
#define FILENAME_MAX_LEN 100

extern int photo_file_num;
extern char file_name_list[DIR_FILE_NUM][DIR_FILE_NAME_MAX_LEN];

enum FILE_TYPE : unsigned char { FILE_TYPE_UNKNOW = 0, FILE_TYPE_FILE, FILE_TYPE_FOLDER };

struct File_Info {
    char *file_name;
    FILE_TYPE file_type;
    File_Info *front_node;
    File_Info *next_node;
};

inline void release_file_info(File_Info *) {}
inline void join_path(char *dst, const char *a, const char *b) {
    if (!dst) return;
    snprintf(dst, FILENAME_MAX_LEN, "%s/%s", a ? a : "", b ? b : "");
}

class SdCard {
public:
    void init() {}
    void listDir(const char *, uint8_t) {}
    // Out-of-line: scans test/fixtures/sd/<dirname>/ on the host
    // filesystem and returns a File_Info linked list mirroring what
    // the firmware's SdCard::listDir builds (head=folder node, then
    // a circular doubly-linked list of file nodes).
    File_Info *listDir(const char *dirname);
    void createDir(const char *) {}
    void removeDir(const char *) {}
    void readFile(const char *) {}
    String readFileLine(const char *, int) { return String(""); }
    void writeFile(const char *, const char *) {}
    File open(const String &, const char * = FILE_READ) { return File(); }
    void appendFile(const char *, const char *) {}
    void renameFile(const char *, const char *) {}
    boolean deleteFile(const char *) { return false; }
    boolean deleteFile(const String &) { return false; }
    void readBinFromSd(const char *, uint8_t *) {}
    void writeBinToSd(const char *, uint8_t *) {}
    void fileIO(const char *) {}
};

#endif
