// Singleton runtime for env:native_ftp. The Arduino/SD/Serial globals
// declared `extern` in the headers need exactly one definition;
// this TU is linked into every test_main.cpp under native/test_ftp_*.

#include "Arduino.h"
#include "SD.h"

HardwareSerial Serial;
unsigned long g_fake_millis = 0;
FakeSD SD;
