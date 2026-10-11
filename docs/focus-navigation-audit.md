# Pager focus audit — 2026-10-11

## Navigation policy

The active application, launcher, or notification must own a usable keyboard/
encoder target. A valid pointer alone is insufficient: hidden or disabled
ancestors, another screen's controls, and deleted targets cannot be restored.
`FormFocus::ensure` repairs invalid focus before input processing and again
after LVGL events, without cancelling editing on an unchanged, usable field.
Scrollable rows remain eligible even when initially outside the viewport.

Rebuilt app views use `FormFocus::View`, including empty-list early returns.
It clears inherited encoder editing and supplies a visible navigation target.
Existing deliberate entry targets (calendar navigation, form title, routine
Next, message Reply/Setup, Settings date/Sync Now) take precedence. Read-only
Today/Transition screens use their root as a navigation target; there is no
artificial action to select on those information-only screens.

Notifications have a separate input group. Both wheel directions wrap within
the modal; keyboard modifiers cannot edit an obscured application field.
Dismissal restores an eligible application target or lets the shell select a
replacement. Queued reminders retain modal input ownership even when an action
callback rebuilds the application underneath.

## Source coverage

| Screen | Paths reviewed and protected |
| --- | --- |
| Launcher | Open, close, return to prior control, stale saved target |
| Today | First entry, resume, read-only fallback |
| Transition | First entry, resume, read-only fallback |
| Quick Capture | Entry, save/result returning to title |
| Calendar | Agenda, empty day, previous/next day, editor, save/Back/delete |
| Tasks | Empty/populated list, editor, save/Back/delete |
| Assignments | List/next-step card, editor, done-step, save/Back/delete |
| Packing | No upcoming event/template, checklist, template list/editor, Back |
| Routines | Empty/populated list, editor, run/next/restart/finish, Back |
| Notes | Empty/populated list, editor, save/Back/delete |
| Timers & Alarms | Preset list, running timer controls, editor, save/Back/delete |
| Habits + Pet | Dashboard, capacity-disabled Add, editor, toggle/save/Back/delete |
| Messages | Inbox, conversation, detail, compose, setup, advanced radio, Back |
| Files | Missing SD, empty folder, nested folder/Up, preview, Back/delete |
| Settings | First entry, time-sync page, return from time sync |
| Notifications | Reminder, confirmation, dismiss/action, queued reminders |
| USB Disk Mode | Inputs intentionally disabled during computer SD ownership; recovery resumes after exit |

Files now always offers Refresh, so even an empty root folder has an actionable
focus target. Its disabled preview textarea is excluded from navigation.
The unused legacy `Dialog::showInfo` helper is not an active application path;
application dialogs use `NotificationManager`.

## Verification

```sh
cmake -S tests/ui_stubs -B /tmp/pocketpda-focus-tests
cmake --build /tmp/pocketpda-focus-tests -j 8
ctest --test-dir /tmp/pocketpda-focus-tests --output-on-failure
```

Tests use real LVGL with production focus helpers, AppManager, InputManager,
and NotificationManager. Synthetic app fixtures exercise lifecycle patterns
for all 14 app IDs; they do not replace physical testing of every app's data
and hardware integrations. Coverage includes repeated launcher/Back cycles,
rebuilding and empty returns, deleted/hidden/disabled targets, full-store Add,
off-screen rows, textarea/dropdown editing, keyboard Back versus delete,
wheel long/short press, modal wrapping, and queued notification actions.

Firmware verification: `pio run -e tlora_pager`.

Quick on-device check after flashing: enter each app, open an item/editor,
return with its Back button, then rotate immediately. Repeat through the
launcher, and dismiss a reminder while an editor is open. The wheel should
work without exiting and reopening the application.
