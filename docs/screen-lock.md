# Optional pager screen lock

This is a casual-access lock: it prevents another child picking up the pager
and navigating the organizer or sending messages. It is not storage encryption
or protection against someone removing the microSD card or reflashing hardware.

## Setup and use

1. Open Settings → LOCK.
2. Enter a six-digit new PIN and repeat it in Confirm New.
3. Choose Lock After Inactivity: 30 seconds, 1, 2, 5, 10 or 15 minutes.
4. Select Save. Lock Now is available immediately.

The default for an existing pager is **off** until its owner sets a PIN. Once
enabled, startup always requires it. Keyboard/wheel activity restarts the
inactivity timeout. Incoming messages, reminder wake-ups and wall-clock changes
do not. Processor light sleep is bounded by the lock deadline as well as
organizer deadlines. The display's dim/off timeouts remain independent.

Enter digits with ALT + the top keyboard row or select the on-screen keypad
with the wheel. Unlock/OK and keyboard Enter submit. DEL or keyboard Back in
the PIN field delete digits. Launcher shortcuts, Back navigation and a held
wheel press cannot leave the lock screen.

Messages, timers, calendar/task reminders and haptics keep working while locked.
Notification content/actions are held behind the lock screen until unlocking.
Haptic alerts and their existing display-wake behavior are immediate, including
simultaneous queued alarms. The lock screen shows a generic "Alert waiting"
indicator without revealing the notification's private content.
USB Disk Mode cannot be started while locked. A USB session already approved
while unlocked is allowed to finish safely; it is not forcibly disconnected by
the timeout. On returning from that session, an expired timeout locks the pager.

Changing the PIN, timeout or disabling the lock requires the current PIN. Leave
both new-PIN fields blank to keep the current PIN while changing the timeout.
There is no automatic data wipe. After three incorrect unlock attempts, retry
delays increase from 5 seconds to a maximum of 80 seconds for the running session.

## Persistence and limitations

The device stores a random salt and SHA-256 PIN digest, not the PIN text, in a
separate NVS namespace. Organizer backups/restores, companion settings exports
and ordinary application-only firmware updates do not overwrite this device's
PIN. No existing data format or pairing keys are changed.

A six-digit PIN and salted hash are not strong offline encryption. Retry delays
reset at reboot, but reboot still requires the PIN. This does not claim secure
boot, flash encryption or resistance to physical hardware attacks.

Keep the PIN somewhere safe. There is deliberately no universal unlock code or
forgot-PIN button; a forgotten PIN or corrupt lock record requires owner/service
recovery. No recovery/erase command is run automatically. Corrupt or unreadable
lock settings fail closed rather than silently disabling an existing lock.

## Verification

`tests/test_screen_lock.cpp` exercises production policy, persistence, UI and
notification code using real LVGL. NVS, random generation and monotonic time are
controlled test adapters; native macOS SHA-256 substitutes for firmware mbedTLS.
Tests cover disabled migration, PIN/timeout validation, reload, change/disable,
write-failure rollback, corrupt records, startup locking, deadline expiry,
wrong-PIN delay, keyboard/encoder group isolation, notification privacy and
wheel-keypad unlocking. Existing focus/input tests also run unchanged.

`test_locked_alert_services.cpp` runs the actual TimerService, ReminderService
and NotificationManager with controlled data/clock adapters: a countdown, two
daily alarms, two calendar reminders and a task reminder all cross the same
deadline while locked. All six haptic alerts fire immediately and all six
notifications remain available after unlocking, without duplicate delivery.
`test_lock_background.py` checks startup credential loading and that the main
background loop, radio RX/ACK/retry/TX paths remain independent of PIN state.
These source/service checks do not replace an over-the-air two-pager test.

Run the native suite via `tests/ui_stubs/CMakeLists.txt`, then build the firmware
with `pio run -e tlora_pager`. On hardware, verify startup, inactivity, Back and
long-wheel bypass prevention, ALT-digit entry and a reminder/message while locked.
