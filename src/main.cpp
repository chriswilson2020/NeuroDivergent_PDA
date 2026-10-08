#include <Arduino.h>
#include "apps/calendar/CalendarApp.h"
#include "apps/clock/ClockApp.h"
#include "apps/files/FilesApp.h"
#include "apps/habits/HabitsApp.h"
#include "apps/notes/NotesApp.h"
#include "apps/settings/SettingsApp.h"
#include "apps/tasks/TasksApp.h"
#include "apps/today/TodayApp.h"
#include "core/PowerManager.h"
#include "core/ReminderService.h"
#include "core/BackupService.h"
#include "core/LowBatteryService.h"
#include "data/CalendarStore.h"
#include "data/HabitStore.h"
#include "data/NoteStore.h"
#include "data/SettingsStore.h"
#include "data/TaskStore.h"
#include "hardware/HardwareManager.h"
#include "hardware/InputManager.h"
#include "hardware/UsbDiskService.h"
#include "ui/Shell.h"

static HardwareManager hardware;
static Shell shell;
static InputManager input;
static PowerManager power;
static SettingsStore settingsStore;
static CalendarStore calendarStore(hardware.storage, hardware.spi);
static HabitStore habitStore(hardware.storage, hardware.spi);
static TaskStore taskStore(hardware.storage, hardware.spi);
static NoteStore noteStore(hardware.storage, hardware.spi);
static UsbDiskService usbDisk(hardware.storage, hardware.spi);
static BackupService backup(hardware.storage, hardware.spi, settingsStore);
static TodayApp today(hardware.rtc, calendarStore);
static CalendarApp calendar(calendarStore, hardware.rtc);
static TasksApp tasks(taskStore, hardware.rtc);
static NotesApp notes(noteStore);
static ClockApp clockApp(hardware.rtc);
static HabitsApp habits(habitStore, hardware.rtc, hardware.haptic);
static FilesApp files(hardware.storage, hardware.spi);
static SettingsApp settings(settingsStore, hardware, power, hardware.rtc, shell, usbDisk, backup);
static ReminderService reminders;
static LowBatteryService lowBattery;
static uint32_t lastMemoryLog = 0;

static void dataRestored(void *) {
    settingsStore.load();
    calendarStore.load();
    habitStore.load();
    taskStore.load();
    noteStore.load();
    power.setConfig(settingsStore.powerConfig());
    hardware.setBrightness(settingsStore.value().brightness);
}

static void usbDiskFinished(void *, bool storageReady) {
    if (!storageReady) {
        shell.notifications().show("SD CARD ERROR", "The SD card could not be remounted after USB Disk Mode.");
        return;
    }
    calendarStore.load();
    habitStore.load();
    taskStore.load();
    noteStore.load();
    if (calendarStore.lastImportCount()) {
        char message[72]; snprintf(message, sizeof(message), "%u events imported and all data reloaded.", static_cast<unsigned>(calendarStore.lastImportCount()));
        shell.notifications().show("AGENDA IMPORTED", message);
    } else {
        shell.notifications().show("USB DISK FINISHED", "SD card ejected safely. Calendar, habits, tasks, and notes reloaded.");
    }
}

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("\nPocketPDA v0.1.0");
    const bool essentialHardwareReady = hardware.begin();
    settingsStore.load();
    const bool storageReady = hardware.storage.mount(hardware.spi);
    if (storageReady) {
        calendarStore.load();
        habitStore.load();
        taskStore.load();
        noteStore.load();
    }
    shell.begin(hardware, today, calendar, tasks, notes, clockApp, habits, files, settings);
    usbDisk.setFinishedCallback(usbDiskFinished, nullptr);
    backup.setRestoredCallback(dataRestored, nullptr);
    input.begin(shell);
    power.begin(hardware, settingsStore.powerConfig());
    reminders.begin(calendarStore, taskStore, hardware.rtc, shell);
    lowBattery.begin(hardware.battery, shell);
    if (calendarStore.lastImportCount()) {
        char message[72]; snprintf(message, sizeof(message), "%u events loaded from the SD card.", static_cast<unsigned>(calendarStore.lastImportCount()));
        shell.notifications().show("AGENDA IMPORTED", message);
    } else if (!essentialHardwareReady) shell.notifications().show("HARDWARE WARNING", "One or more essential devices were not detected. See the serial log.");
    else if (!storageReady) shell.notifications().show("STORAGE WARNING", "microSD was not mounted. Calendar, habits, tasks, and notes cannot be saved.");
}

void loop() {
    usbDisk.update();
    if (!usbDisk.active()) {
        hardware.update();
        shell.update();
        power.update();
        reminders.update();
        lowBattery.update();
    }
    lv_timer_handler();
    if (millis() - lastMemoryLog >= 60000) {
        lastMemoryLog = millis();
        Serial.printf("[PocketPDA] heap=%u min=%u psram=%u\n", ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getFreePsram());
    }
    delay(5);
}
