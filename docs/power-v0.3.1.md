# Power management v0.3.1

Status: software implementation; physical acceptance pending. Processor sleep
is OFF by default. No firmware was flashed during this change.

## Hardware and feasibility

Checked against pinned LilyGoLib `0015b6a`, board variant, and its schematic
`T-Lora Pager V1.0 SCH 25-06-13.pdf`, sheets 3 and 5.

| Source | GPIO | Experimental light-sleep wake |
| --- | --- | --- |
| SX1262 DIO1 | 14 | GPIO HIGH level |
| TCA8418 keyboard INT | 6 | GPIO LOW level |
| PCF85063 RTC INT | 1 | GPIO LOW level |
| Wheel A/B | 40/41 | GPIO opposite the sampled level |
| Wheel press | 7 | GPIO LOW level |
| BOOT | 0 | GPIO LOW level |
| Scheduled work | Internal RTC timer | Maximum 100 ms sleep interval |

The SX1262 has an independent clock, buffer and power rail, so CPU light sleep
is compatible in principle with continuous radio RX. The implementation leaves
radio RX and keyboard power intact. DIO1 is a latched packet interrupt; pending
radio, keyboard or RTC levels inhibit entry. Existing ISRs remain installed.
ESP32-S3 GPIO wake supports digital GPIO40/41; EXT1 is limited to RTC IO and
cannot cover the wheel. PWR is a hardware power switch, not a programmable input.

Do not call LilyGoLib's `lightSleep()`: it calls `radio.sleep()`, `kb.end()`,
disables rotary input, unmounts SD and cuts keyboard power. It also wakes the
display afterward. PocketPDA uses the IDF API directly, without rail changes
during CPU sleep or extra radio beacons.

Sources: [LILYGO schematic](https://github.com/Xinyuan-LilyGO/LilyGoLib/blob/master/schematic/T-Lora%20Pager%20V1.0%20SCH%2025-06-13.pdf),
[LILYGO hardware reference](https://github.com/Xinyuan-LilyGO/LilyGoLib/blob/master/docs/hardware/lilygo-t-lora-pager.md),
[ESP-IDF S3 sleep API](https://docs.espressif.com/projects/esp-idf/en/v5.5.1/esp32s3/api-reference/system/sleep_modes.html).

## Baseline and fallback

Original firmware dims at 30 seconds and sleeps the display at 120 seconds,
or the user's saved settings. Backlight and keyboard illumination go off;
the CPU continues a 5 ms polling loop. Radio RX, keyboard, SD, haptic, expander,
RTC, gauge and charger remain powered. Sensor/codec supplies are not gated.
LilyGoLib initially enables all expander rails, including GPS, NFC and amplifier,
even when those devices' initialization was skipped.

v0.3.1 explicitly disables the unused GPS/NFC/speaker-amplifier rails after
initialization. Current PocketPDA apps do not use them. Radio, keyboard, SD and
haptic power are retained. The API's sensor power control is a no-op, so that
rail is not changed. Charger/PMIC initialization, current/voltage limits and
gauge calibration are unchanged.

Default display-only fallback sleeps cooperatively for 20 ms per background
loop (5 ms active). This reduces polling frequency while maintaining RX and
input processing. Current savings have not been measured.

## Scheduling and clocks

The coordinator selects the earliest countdown, daily alarm, calendar/task or
snooze deadline and owns the hardware RTC alarm. It recalculates at least every
250 ms; experimental CPU sleep is capped at 100 ms, preserving radio retry and
input processing. It skips CPU sleep during TX, pending IRQs, USB charging,
active serial CDC, USB Disk Mode, and invalid RTC/gauge state. Unexpected API
failures disable the experiment until reboot.

Running countdowns use monotonic `esp_timer_get_time()`, with the existing
absolute finish timestamp retained across reboot. Explicit RTC edits rebase
the persisted deadline without changing remaining duration. Expired persisted
countdowns are processed on the first service check. Alarms/reminders process
crossed deadlines rather than requiring a check inside a 60-second window.
All matching daily alarms run; calendar checks include adjacent dates for
midnight advance reminders. Intentional clock edits reset the wall-event cursor
to the selected time rather than replaying all skipped appointments. Existing
timezone policy is unchanged.

Notifications now queue FIFO (64 pending slots) so simultaneous due items do
not overwrite each other. Overflow is reported on serial. Snooze copies the
selected notification's content. Very long interruptions spanning several days
are not a historical calendar replay; normal light-sleep intervals are bounded.

LVGL's pinned helper obtains ticks from `millis()`, and Arduino obtains millis
from `esp_timer_get_time()`. IDF compensates this timer over light sleep. Do not
call `lv_tick_inc()` on wake: elapsed time would be counted twice. The external
PCF85063 stays powered. LVGL timer callbacks resume after wake; deadline timing
does not depend on animation callbacks. Experimental telemetry compares sleep
duration with tick advance and disables CPU sleep if ticks stall.

## Instrumentation and configuration

Defaults in `src/core/PowerOptions.h` can be overridden in PlatformIO build flags:

```
-D POCKETPDA_EXPERIMENTAL_LIGHT_SLEEP=0
-D POCKETPDA_UNUSED_RAILS_OFF=1
-D POCKETPDA_POWER_DIAGNOSTICS=1
-D POCKETPDA_POWER_SD_LOG=1
```

Only development images should set experimental sleep to 1. Set unused rails
off to 0 for the old rail baseline. Set both logging flags to 0 to disable new
power instrumentation. Existing battery/memory/message serial logs remain.

Every minute, serial and `/PocketPDA/logs/power.csv` record epoch, uptime,
display state/time, radio RX/TX/sleep time, CPU awake/sleep time, wake count/cause,
packet counts, SOC, voltage, signed reported current, USB/charging, gauge validity
and sleep errors. Counters reset on reboot; uptime identifies sessions. Wake
causes use IDF enum values. Serial `[wake]` also records the sampled active GPIO
bitmask (not a latched hardware cause; brief wheel pulses can be absent), sleep
duration and LVGL tick advance. `[deadline]` records event ID and lateness.
No message contents or encryption keys are recorded by this instrumentation.

The CSV is closed after each append. At 1 MiB it rotates to
`power.previous.csv`, retaining one older generation (about 2 MiB total).
Writes are blocked while USB owns the card. Download logs via USB Disk Mode.
SD failure leaves serial logging available. Logging creates brief SD activity;
use identical logging settings for before/after current comparisons.

## Verification, limitations and acceptance

Native tests cover midnight/DST, crossed deadlines, late wake, simultaneous
deadlines and budgets. Tests of the actual TimerService cover multiple daily
alarms, delayed checks, clock edits, countdown expiration and persisted expiry.
Existing messaging tests are retained. Build success is software verification.

A USB pager was detected, but a one-minute serial read produced no samples.
No new baseline current was obtained. Earlier USB charging logs are not
battery-only standby measurements. Before/after current and savings are
**unmeasured**, and all physical acceptance items remain pending:

- Two pagers exchanging messages, ACKs and retries with both displays off.
- Radio wake, burst packet retention and acceptable ACK latency.
- Keyboard first keystroke, fast wheel edges and repeated sleep/wake cycles.
- 15-minute countdown with incoming messages, daily alarms and simultaneous
  calendar/task reminders including snooze/dismiss and midnight.
- RTC accuracy, reboot persistence, LVGL recovery, SPI/PSRAM retention.
- USB reconnect/charging, USB Disk Mode, shutdown/restart.
- Lower measured battery-only standby current than the baseline.

Uncertainties: board revisions may differ; wheel edges can be lost during CPU
wake latency; receive buffers can be overwritten by bursts; USB insertion is
polled (up to the 1-second charging-status interval); concurrent library tasks
and pending SPI activity need physical validation. Framework flash/PSRAM
retention policy is left unchanged. No claim of loss-free reception or input
reliability is made from schematic review or compilation.

Measure baseline/candidate on the same unit, comparable SOC, screen/radio
settings and schedule. Disconnect USB for discharge measurements, collect SD
logs or use an external current meter, and run at least 30 minutes plus the
15-minute timer/message tests. Compare reported signed current against a meter
where possible. Repeat on both units. Keep processor sleep disabled by default
until every physical acceptance test passes.
