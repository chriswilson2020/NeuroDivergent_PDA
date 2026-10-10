#pragma once
#include <stdint.h>
#include <cstring>
extern uint32_t fakeMs;
inline uint32_t millis(){return fakeMs;}
inline size_t strlcpy(char *dst,const char *src,size_t n){size_t len=strlen(src);if(n){size_t copy=len<n-1?len:n-1;memcpy(dst,src,copy);dst[copy]=0;}return len;}
