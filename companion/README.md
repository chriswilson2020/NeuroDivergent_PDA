# PocketPDA Companion for macOS

PocketPDA Companion is a native macOS application for a Pager running PocketPDA. Version 0.1 detects the microSD volume exposed by **Settings > USB DISK** and provides:

- one-way synchronization from selected macOS calendars, including Outlook calendars added to macOS;
- a selectable 4, 12, or 26 week synchronization window;
- preservation of a previous CSV-imported base schedule;
- manual PocketPDA calendar CSV import;
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

Calendar synchronization writes `/PocketPDA/calendar/import.csv`, which the firmware validates and imports after USB Disk Mode ends. **Request Backup & Eject** first asks where the new archive should be saved, then writes a one-shot request and safely ejects. Firmware v0.2.5 or newer consumes the request and creates the backup after regaining exclusive microSD access. Re-enter USB Disk Mode when the Pager says **BACKUP READY**; the companion validates the new archive and automatically saves it to the chosen location. The prior on-device archive is retained as `PocketPDA-Backup.previous.ppb` until the next request. A staged restore is never applied automatically; the user must eject the device and confirm **RESTORE** on the Pager.

The initial release uses USB Disk Mode for broad compatibility. A future version can add a framed USB command protocol for fully automatic backup creation without mounting the microSD card.
