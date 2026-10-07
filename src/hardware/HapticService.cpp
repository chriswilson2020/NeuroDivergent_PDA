#include "HapticService.h"
#include <LilyGoLib.h>

void HapticService::play(uint8_t effect) {
    if (!available_) return;
    instance.drv.setWaveform(0, effect);
    instance.drv.setWaveform(1, 0);
    instance.drv.run();
}
