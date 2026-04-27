#ifndef AIO_STUB_FS_H
#define AIO_STUB_FS_H
#include "Arduino.h"
#include <stdio.h>
#include <ctime>

#define FILE_READ "r"
#define FILE_WRITE "w"
#define FILE_APPEND "a"

namespace fs {

class File {
private:
    FILE *fp = nullptr;
    String fname;
    bool isDir = false;
public:
    File() {}
    File(FILE *f, const String &n, bool d = false) : fp(f), fname(n), isDir(d) {}
    operator bool() const { return fp != nullptr || isDir; }
    bool isDirectory() const { return isDir; }
    int available() {
        if (!fp) return 0;
        long pos = ftell(fp);
        fseek(fp, 0, SEEK_END);
        long end = ftell(fp);
        fseek(fp, pos, SEEK_SET);
        return (int)(end - pos);
    }
    size_t read(uint8_t *buf, size_t n) { return fp ? fread(buf, 1, n, fp) : 0; }
    int read() { if (!fp) return -1; int c = fgetc(fp); return c == EOF ? -1 : c; }
    size_t size() {
        if (!fp) return 0;
        long pos = ftell(fp);
        fseek(fp, 0, SEEK_END);
        long s = ftell(fp);
        fseek(fp, pos, SEEK_SET);
        return (size_t)s;
    }
    bool print(const char *s) { return fp ? fwrite(s, 1, strlen(s), fp) > 0 : false; }
    size_t write(const uint8_t *buf, size_t n) { return fp ? fwrite(buf, 1, n, fp) : 0; }
    size_t write(uint8_t b) { return fp ? fwrite(&b, 1, 1, fp) : 0; }
    void close() { if (fp) { fclose(fp); fp = nullptr; } }
    const char *name() const { return fname.c_str(); }
    File openNextFile() { return File(); }
    time_t getLastWrite() { return 0; }
    void flush() { if (fp) fflush(fp); }
};

class FS {
public:
    File open(const char *, const char * = "r") { return File(); }
    File open(const String &, const char * = "r") { return File(); }
    bool exists(const char *) { return false; }
    bool remove(const char *) { return false; }
    bool rename(const char *, const char *) { return false; }
    bool mkdir(const char *) { return false; }
    bool rmdir(const char *) { return false; }
};

}

using fs::File;
using fs::FS;

#endif
