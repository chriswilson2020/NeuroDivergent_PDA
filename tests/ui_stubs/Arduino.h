#pragma once
#include <stdint.h>
#include <string.h>
struct FocusTestSerial { void println(const char *) {} };
inline FocusTestSerial Serial;
