#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Strict, bounded text protocol. The session is generated before handing SD to USB;
// a request from a previous USB session must never reset the clock later.
namespace CompanionTimeRequest {
inline bool parse(const char *text, size_t size, const char *session, int64_t &epoch) {
    const char *magic = "PocketPDA-Time-1\n";
    const size_t prefix = strlen(magic), token = strlen(session);
    if (!token || size < prefix + token + 3 || size > 160 ||
        memcmp(text, magic, prefix) || memcmp(text + prefix, session, token) ||
        text[prefix + token] != '\n') return false;
    size_t pos = prefix + token + 1;
    int64_t value = 0;
    size_t digits = 0;
    while (pos < size && text[pos] >= '0' && text[pos] <= '9') {
        if (++digits > 10) return false;
        value = value * 10 + text[pos++] - '0';
    }
    if (pos + 1 != size || text[pos] != '\n' || digits != 10 ||
        value < 1577836800LL || value > 4102444799LL) return false;
    epoch = value;
    return true;
}
}
