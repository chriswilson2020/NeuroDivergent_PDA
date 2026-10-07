#include "SPIBusManager.h"
#include <LilyGoLib.h>
bool SPIBusManager::lock(TickType_t wait) { return instance.lockSPI(wait); }
void SPIBusManager::unlock() { instance.unlockSPI(); }
