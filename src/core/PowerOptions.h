#pragma once
#ifndef POCKETPDA_UNUSED_RAILS_OFF
#define POCKETPDA_UNUSED_RAILS_OFF 1
#endif
#ifndef POCKETPDA_EXPERIMENTAL_LIGHT_SLEEP
#define POCKETPDA_EXPERIMENTAL_LIGHT_SLEEP 1
#endif
// Maintenance ceiling only; organizer deadlines and GPIO wake can shorten it.
#ifndef POCKETPDA_LIGHT_SLEEP_MAX_MS
#define POCKETPDA_LIGHT_SLEEP_MAX_MS 30000
#endif
static_assert(POCKETPDA_LIGHT_SLEEP_MAX_MS >= 1 && POCKETPDA_LIGHT_SLEEP_MAX_MS <= 30000,
              "Longer maintenance gaps require a separate background-service audit");
#ifndef POCKETPDA_POWER_DIAGNOSTICS
#define POCKETPDA_POWER_DIAGNOSTICS 1
#endif
#ifndef POCKETPDA_POWER_SD_LOG
#define POCKETPDA_POWER_SD_LOG 1
#endif
