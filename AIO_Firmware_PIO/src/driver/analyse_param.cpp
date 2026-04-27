// Line-splitter underlying every config parser in the firmware
// (read_config(SysUtilConfig/SysMpuConfig/RgbConfig) plus the per-app
// .cfg readers in stockmarket / weather / picture / etc).
//
// Lifted out of driver/flash_fs.cpp so the host-side Unity unit test
// (env:native_unit) can compile just this TU — flash_fs.cpp's other
// methods all touch SPIFFS / FS, which would drag heavy stubs into
// the unit-test binary for no testing benefit.
//
// Behavior contract (preserved verbatim from the original):
// * The buffer is mutated in place — each '\n' is overwritten with
//   '\0' so each argv[i] is a NUL-terminated C string.
// * The caller must guarantee `info` contains at least `argc`
//   newlines. With fewer, the inner while-loop walks past the end
//   of the buffer (undefined behavior). Firmware write_config
//   implementations always emit exactly the right number of
//   newlines, so this is fine in practice.

#include "flash_fs.h"

bool analyseParam(char *info, int argc, char **argv)
{
    int cnt;
    for (cnt = 0; cnt < argc; ++cnt)
    {
        argv[cnt] = info;
        while (*info != '\n')
        {
            ++info;
        }
        *info = 0;
        ++info;
    }
    return true;
}
