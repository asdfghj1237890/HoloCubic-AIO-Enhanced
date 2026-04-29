#ifndef AIO_FTP_STUB_FS_H
#define AIO_FTP_STUB_FS_H
// Minimal namespace shim. ESP32FtpServer.cpp doesn't reference fs::*
// directly; SD.h pulls this in for File typedef compatibility.
namespace fs { class FS {}; }
#endif
