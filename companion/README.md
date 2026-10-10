# PocketPDA Companion for macOS

PocketPDA Companion is a native macOS application for a Pager running PocketPDA. Version 0.1 detects the microSD volume exposed by **Settings > USB DISK** and provides:

- one-way synchronization from selected macOS calendars, including Outlook calendars added to macOS;
- a selectable 4, 12, or 26 week synchronization window;
- preservation of a previous CSV-imported base schedule;
- manual PocketPDA calendar CSV import;
- setting the pager's date and time from the Mac, preserving its timezone;
- backup requests initiated from the Mac, verified backup download, and verified restore staging;
- safe device ejection; and
- access to the offline organizer editor.

## Build

```sh
chmod +x companion/macos/build.sh
companion/macos/build.sh
```

The signed Universal application is created at `dist/PocketPDA Companion.app`. It runs on Apple Silicon and Intel Macs with macOS 14 or newer. On first calendar synchronization, macOS asks for calendar access. Outlook calendars are available after adding the Outlook account under **System Settings > Internet Accounts** and enabling Calendars.

## Safety model

Calendar synchronization writes `/PocketPDA/calendar/import.csv`, which the firmware validates and imports after USB Disk Mode ends. With companion **v0.1.5 and matching updated firmware**, **Back Up to Mac…** asks where to save, then streams the organizer files directly from the mounted SD card into a new Mac archive. The pager exports a small, checksummed settings snapshot immediately before entering USB Disk Mode, bound to the current USB session. The app includes this snapshot, verifies every archived file, and only then atomically installs the destination backup. Existing Mac backups remain unchanged on copy or verification failure. No on-device backup button, ejection or reconnection is required. The app does not move or delete previous on-device archives.

Older companion versions used an eject/create/reconnect request workflow. Existing pending downloads can still finish if their requested archive is available; otherwise use **Cancel Pending Download** to clear the stale app state without deleting any backup or organizer data. A fresh direct backup is never blocked by a pending old download. A staged restore is never applied automatically; the user must eject the device and confirm **RESTORE** on the Pager.

## Set time from Mac (companion v0.1.4)

Install the firmware built alongside this companion update, then enter **Settings > USB DISK**. Click **Set Pager Time from Mac**. The app writes the Mac's UTC Unix timestamp and safely ejects. The pager consumes the request once, verifies its RTC write, and displays **MAC TIME SYNC**. Re-enter USB Disk Mode to read the verified success/failure result in the app. No internet, satellite fix, calendar replacement, or timezone change is involved. For Amsterdam local time, keep **Europe/Amsterdam** selected on the pager. Check that the Mac itself has correct date and time first.

This is a manual USB handoff, not subsecond network synchronization: the clock may lag by the few seconds between writing the request and applying it after ejection. Eject failures should be resolved by clicking time sync again before ejecting, rather than leaving a staged timestamp for later. A fresh device-specific random session is published before each USB handoff; the firmware rejects requests from another session or pager, malformed timestamps, and dates outside 2020–2099. It does not apply requests found at boot. Use the application-only updater (`tools/flash_update.py`), not a factory flash, to preserve existing NVS preferences.

The app uses USB Disk Mode for broad compatibility. Direct backups are streamed with bounded buffers and use the pager's PDB1 format, including its current 128-entry and 87-byte UTF-8 path limits. Hidden Mac metadata and `@eaDir` thumbnails are excluded; symbolic links and unsupported paths are rejected rather than silently followed. The firmware never accesses the card while the Mac owns it.
