# Light-sleep architecture, transition revision 2

Included in stable v0.3.2, 2026-10-10. The default `tlora_pager` enables
processor sleep with `POCKETPDA_LIGHT_SLEEP_MAX_MS=30000`.
The filename and legacy configuration/CSV names are retained for compatibility.
See [current power-management usage](power-v0.3.2.md).

Startup serial output identifies
`max_sleep_ms`; existing CSV schemas are unchanged, and `planned_us`/`sleep_ms`
show the actual requested/returned durations.

This changes only the maintenance cap, not the interrupt transaction, RX/ACK
path or deadline calculation. Radio, keyboard, wheel and RTC wake remain armed.
Battery status polling (normally 1 s), gauge refresh (5 s), low-battery checks
(2 s), USB detection and non-deadline UI/background work resume on the next wake:
while sleeping they may wait up to about thirty seconds plus processing overhead.
Charging is hardware-controlled, not dependent on those polling intervals.
No Wi-Fi/Bluetooth connection is active. This is not approval for a 300-second
gap; longer gaps require explicit background-service scheduling. The minute
logs run on the next maintenance wake and can be up to thirty seconds apart from
an exact minute boundary. Instantaneous current samples taken while awake are
not whole-cycle average current; compare SOC over hours and CPU sleep duty cycle.

Charger polling is status-only, and USB Disk Mode cannot start
without an awake UI action. Reminder deadlines, pending ACK/retries and outgoing
queues retain their existing sleep vetoes; no background polling exception is
being claimed as a hard deadline. CPU sleep still requires unplugged, screen-off
idle. USB insertion alone is not an armed GPIO wake source: press a key/wheel to
wake immediately, or wait for maintenance to detect it.

Build with `pio run -e tlora_pager`. Display-only troubleshooting uses
`tlora_pager_awake`; `tlora_pager_sleep_test` is a compatibility alias.

## References and scope

Reviewed LILYGO Meshtastic's
[sleep.cpp](https://github.com/Xinyuan-LilyGO/Meshtastic_firmware/blob/master/src/sleep.cpp)
(`doPreflightSleep`, `doLightSleep`, `enableLoraInterrupt`) and
[PowerFSM.cpp](https://github.com/Xinyuan-LilyGO/Meshtastic_firmware/blob/master/src/PowerFSM.cpp)
(`lsEnter`, `lsIdle`, `lsExit`) on 2026-10-10. Adapted the mechanisms, not their
PowerFSM, transport, periodic LED blink or blocking preflight wait loop.

The first PocketPDA experiment configured GPIO level wake while the normal
edge-triggered ISRs were still enabled. In the pinned IDF 5.5.5
[GPIO driver](https://github.com/espressif/esp-idf/blob/v5.5.5/components/esp_driver_gpio/src/gpio.c),
`gpio_wakeup_enable` overwrites the normal interrupt trigger type, while
`gpio_wakeup_disable` only disables wake. This is a verified software defect
and a plausible cause of the reported slow wake/missed reception. The six-row
power log does not prove it caused that physical failure: counters were written
after sleep returned, and zero completed sleeps does not rule out a stalled
transition. The old experimental image should not be reused.

## Preflight and transition

1. Require display-off, enabled processor sleep, valid RTC/gauge, no USB power,
   active CDC or USB Disk Mode. Veto if keyboard/wheel press/BOOT is held.
2. Require radio RX with no software IRQ, asserted DIO1, transmit, pairing,
   outstanding message/ACK/retry or queued outgoing work. This deliberately
   prioritizes the unchanged transport over additional power savings.
3. Read the RTC and freshly scan SD-backed calendar reminders and RAM tasks,
   including snooze. A failed scan/checksum vetoes sleep. Use the last processed
   service time so an overdue but undispatched reminder is not discarded.
4. Calculate a monotonic wake deadline (below). Clear an already asserted RTC
   alarm and return to normal processing rather than sleeping on a low IRQ.
5. Write the first candidate-entry marker for this display-off episode, then
   acquire SPI nonblocking. If another owner holds SPI, defer to another loop.
6. Snapshot each wake pin's normal interrupt type, CPU-enabled state and sleep
   selection. Mask CPU interrupt delivery on ALL these pins before installing
   level-sensitive wake. Registered callbacks remain installed. The S3 adapter
   reads `GPIO.pin[].int_type/int_ena` because the pinned public driver has no
   getters; restoration uses IDF APIs, including `gpio_set_intr_type` to restore
   driver edge-clear bookkeeping. No whole GPIO register is overwritten.
7. Arm GPIO wake and the internal timer. Recheck radio software/DIO1, every armed
   wake level and the remaining budget after setup, immediately before entry.
   An IRQ/input during setup vetoes sleep; a latched DIO1 after the final check
   is an armed HIGH-level wake condition. No IRQ status or radio buffer is
   cleared by preflight.
8. Call `esp_light_sleep_start()` directly. Never invoke LilyGoLib light sleep,
   turn the SX1262 off, disable its receive mode or reset keyboard/rotary queues.
9. Latch DIO1 into the existing radio software IRQ before and after restoration.
   Disable owned wake sources, restore ALL trigger types/sleep selections, then
   re-enable only interrupts that were previously enabled. The RAII guard also
   unwinds partial setup, preflight veto and sleep rejection. It leaves unrelated
   wake-source types untouched. This coordinator is the sole GPIO/timer sleep
   wake owner; additional owners would need an explicit shared ownership API.
10. Release SPI and service normal RX/ACK handling before timer/reminder work.
    User-input wake marks LVGL activity and restores the display, without
    synthesizing a character or click. Never restart RX merely because sleep
    returned; the existing transport owns packet read/decrypt/deduplication/ACK.

Any sleep/setup/restore API failure disables CPU sleep until
reboot. Failed type restoration does NOT re-enable potentially level-triggered
ISRs; the error log requests a restart. This avoids an interrupt storm, but
normal input/radio operation cannot be promised after a hardware/driver restore
failure. The error code is retained in the diagnostic log.

PocketPDA currently does not start Wi-Fi or Bluetooth. Future features enabling
either must add a preflight veto or coordinated shutdown before explicit sleep;
connections are not retained by this sleep implementation.

## GPIO contract

| Source | GPIO | Sleep wake | Awake handling |
| --- | --- | --- | --- |
| SX1262 DIO1 | 14 | HIGH level | Restore saved normal edge ISR; latch pending DIO1 |
| Keyboard INT | 6 | LOW level | Restore saved CHANGE handler; keep TCA8418 FIFO |
| PCF85063 RTC INT | 1 | LOW level | Restore saved FALLING handler; service stored deadlines |
| Wheel A/B | 40 / 41 | Opposite sampled resting level | Restore prior configuration; keep rotary task/queue |
| Wheel press | 7 | LOW level | Restore prior configuration; no synthetic click |
| BOOT | 0 | LOW level | Restore prior configuration |

GPIO wake covers both RTC and digital pins, including 40/41, with mixed
polarities. Unlike Meshtastic's optional DIO1 EXT0 route, no pin is switched to
RTC mux mode, so EXT0 hold/mux restoration is unnecessary. No pull, direction,
radio CS/reset, peripheral power or flash/PSRAM power-down policy is changed.
See [ESP-IDF S3 sleep documentation](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32s3/api-reference/system/sleep_modes.html).

## Deadlines and LVGL

Countdowns contribute their exact `esp_timer_get_time()` finish instant, not
rounded-up remaining seconds. Daily alarms, calendar reminders, task reminders
(the existing 09:00 due-date policy) and snooze contribute their earliest pending
wall-clock deadline. A fresh second-resolution RTC sample is anchored to the
monotonic time taken BEFORE the read. Subtract one second of quantization
uncertainty, elapsed preflight/SD/setup time and a 2 ms entry margin.

The internal timer uses the minimum of that deadline and the configured
housekeeping ceiling (thirty seconds). The ceiling only shortens sleep; it cannot override a
nearer deadline. Due, overdue or too-close deadlines veto sleep. Budget is
rechecked at entry. Existing 250 ms timer and one-second reminder dispatch
granularities still apply; this is not a hard real-time latency guarantee.
Fresh scans can prevent sleep if they exceed the maintenance budget; measure
scan cost with large agendas before lengthening intervals or caching results.

The external RTC stays powered. LVGL's tick callback uses `millis()`, backed by
IDF's sleep-compensated monotonic timer. Compare actual `lv_tick_get()` advance
against elapsed sleep and disable processor sleep if it stalls. Never manually
increment LVGL ticks. Animation callbacks resume on the next service pass.

## Diagnostics and tests

Existing `/PocketPDA/logs/power.csv` remains schema-compatible. Experimental
builds additionally write `/PocketPDA/logs/sleep.csv`:

- `enter`: first candidate entry per display-off episode, closed before masking
  normal interrupts. It is an intent marker, not proof sleep was entered.
- `return`: first returned sleep attempt in the boot session; errors also record
  a return when entry was attempted. Other wake results appear in minute samples.
- `sample`: once per minute, including completed attempts/wakes, latest blocked
  mask, sampled wake levels, last sleep/tick durations and errors.
- `error`: immediately on an API failure. `last_error` is the IDF return code.

Blocked-mask bits: 1 disabled, 2 display on, 4 USB/CDC, 8 RTC, 16 gauge,
32 radio busy/pending, 64 input or asserted external RTC, 128 reminder scan,
256 SPI busy, 512 deadline due/too close, 1024 IRQ/input during transition.
GPIO masks are sampled levels, NOT a latched S3 wake-history register. Short
wheel pulses may have ended before sampling. Logs rotate at 1 MiB with one
previous file; missing SD leaves serial diagnostics. No writes occur while USB
Disk Mode owns the card. Logging and repeated scans themselves cost energy.

Native checks (all passed during implementation):

```
c++ -std=c++17 -Isrc tests/test_sleep_transition.cpp -o /tmp/pocketpda-sleep-transition
/tmp/pocketpda-sleep-transition
c++ -std=c++17 -Isrc tests/test_deadlines.cpp -o /tmp/pocketpda-deadlines
/tmp/pocketpda-deadlines
c++ -std=c++17 -DPOCKETPDA_POWER_DIAGNOSTICS=0 -Itests/power_stubs -Isrc tests/test_timer_service.cpp src/core/TimerService.cpp -o /tmp/pocketpda-timers
/tmp/pocketpda-timers
c++ -std=c++17 -DPOCKETPDA_POWER_DIAGNOSTICS=0 -Itests/power_stubs -Isrc tests/test_reminder_sleep.cpp src/core/ReminderService.cpp -o /tmp/pocketpda-reminder-sleep
/tmp/pocketpda-reminder-sleep
python3 -m unittest discover -s tests
pio run -e tlora_pager -e tlora_pager_sleep_test
```

Tests exercise the production wake transaction with a fake backend: every
setup failure, early veto/rejected sleep unwind, failed restore stays masked,
all wheel resting states, latched IRQ/input during transition, budget exhaustion,
precise countdowns, fresh calendar/task edits, overdue reminders and concurrent
reminders. Existing protocol-model tests remain; these are not an RF reception
test or proof of ESP32 driver behaviour under physical interrupts.

SX1262 buffer overwrites during bursts and fast wheel edges remain hardware
limitations. Display-only troubleshooting firmware can be uploaded without
erasing settings or SD data.
