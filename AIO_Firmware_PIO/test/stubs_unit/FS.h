#ifndef AIO_UNIT_STUB_FS_H
#define AIO_UNIT_STUB_FS_H
// flash_fs.h #includes <FS.h> at the top, but analyse_param.cpp
// doesn't actually USE any of it — only the analyseParam declaration
// at the bottom matters here. An empty stub is enough to satisfy
// the include chain for the unit-test build.
namespace fs { class FS {}; }
#endif
