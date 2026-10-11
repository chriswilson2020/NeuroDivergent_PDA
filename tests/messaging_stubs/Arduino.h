#pragma once
#include <stdint.h>
#include <string.h>
struct TestESP { uint64_t getEfuseMac()const{return 123;} };
inline TestESP ESP;
