#pragma once
#include <CommonCrypto/CommonDigest.h>
// Native macOS SHA-256 backend; production firmware uses mbedTLS.
inline int mbedtls_sha256(const unsigned char *input,size_t size,unsigned char *output,int) {
    return CC_SHA256(input,static_cast<CC_LONG>(size),output)?0:-1;
}
