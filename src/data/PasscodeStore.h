#pragma once
#include <stdint.h>

// Device-local lock settings are deliberately outside organizer backups and
// companion exports. Restoring Gabriel's data must not change Daddy's PIN.
class PasscodeStore {
public:
    bool load();
    bool enabled() const { return fault_||record_.enabled; }
    bool fault() const { return fault_; }
    uint16_t timeoutSeconds() const { return record_.seconds; }
    bool verify(const char *pin) const;
    bool change(const char *current,const char *next,uint16_t seconds);
    static bool validPin(const char *pin);
    static bool validTimeout(uint16_t seconds);
private:
    struct Record {
        uint32_t version=1;
        uint16_t seconds=120;
        uint8_t enabled=0,reserved=0;
        uint8_t salt[16]{},digest[32]{};
    } record_{};
    bool fault_=false;
    static bool digest(const char *pin,const uint8_t *salt,uint8_t *out);
};
