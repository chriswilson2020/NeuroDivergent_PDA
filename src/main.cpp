#include <Arduino.h>
#include "apps/calendar/CalendarApp.h"
#include "apps/assignments/AssignmentsApp.h"
#include "apps/capture/CaptureApp.h"
#include "apps/clock/ClockApp.h"
#include "apps/files/FilesApp.h"
#include "apps/habits/HabitsApp.h"
#include "apps/notes/NotesApp.h"
#include "apps/packing/PackingApp.h"
#include "apps/routines/RoutinesApp.h"
#include "apps/settings/SettingsApp.h"
#include "apps/tasks/TasksApp.h"
#include "apps/today/TodayApp.h"
#include "apps/transition/TransitionApp.h"
#include "core/PowerManager.h"
#include "core/ReminderService.h"
#include "core/TimerService.h"
#include "core/BackupService.h"
#include "core/LowBatteryService.h"
#include "data/CalendarStore.h"
#include "data/AssignmentStore.h"
#include "data/HabitStore.h"
#include "data/NoteStore.h"
#include "data/PackingStore.h"
#include "data/RoutineStore.h"
#include "data/SettingsStore.h"
#include "data/TaskStore.h"
#include "data/TimerStore.h"
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
static AssignmentStore assignmentStore(hardware.storage, hardware.spi);
static HabitStore habitStore(hardware.storage, hardware.spi);
static TaskStore taskStore(hardware.storage, hardware.spi);
static NoteStore noteStore(hardware.storage, hardware.spi);
static PackingStore packingStore(hardware.storage, hardware.spi);
static RoutineStore routineStore(hardware.storage, hardware.spi);
static TimerStore timerStore(hardware.storage, hardware.spi);
static UsbDiskService usbDisk(hardware.storage, hardware.spi);
static BackupService backup(hardware.storage, hardware.spi, settingsStore);
static TodayApp today(hardware.rtc, calendarStore);
static TransitionApp transition(hardware.rtc, calendarStore);
static CaptureApp capture(taskStore, noteStore, hardware.rtc);
static CalendarApp calendar(calendarStore, hardware.rtc);
static TasksApp tasks(taskStore, hardware.rtc);
static AssignmentsApp assignments(assignmentStore, hardware.rtc, hardware.haptic);
static PackingApp packing(packingStore, calendarStore, hardware.rtc, hardware.haptic);
static NotesApp notes(noteStore);
static RoutinesApp routines(routineStore, hardware.haptic);
static TimerService timerService;
static ClockApp clockApp(hardware.rtc, timerStore, timerService);
static HabitsApp habits(habitStore, hardware.rtc, hardware.haptic);
static FilesApp files(hardware.storage, hardware.spi);
static SettingsApp settings(settingsStore, hardware, power, hardware.rtc, shell, usbDisk, backup);
static ReminderService reminders;
static LowBatteryService lowBattery;
static uint32_t lastMemoryLog = 0;

static void dataRestored(void *) {
    settingsStore.load();
    calendarStore.load();
    assignmentStore.load();
    habitStore.load();
    taskStore.load();
    noteStore.load();
    packingStore.load();
    routineStore.load();
    timerStore.load();
    timerService.reload();
    power.setConfig(settingsStore.powerConfig());
    hardware.setBrightness(settingsStore.value().brightness);
}

static void usbDiskFinished(void *, bool storageReady) {
    if (!storageReady) {
        shell.notifications().show("SD CARD ERROR", "The SD card could not be remounted after USB Disk Mode.");
        return;
    }
    calendarStore.load();
    assignmentStore.load();
    habitStore.load();
    taskStore.load();
    noteStore.load();
    packingStore.load();
    routineStore.load();
    timerStore.load();
    timerService.reload();
    const size_t eventImports = calendarStore.lastImportCount();
    const size_t taskImports = taskStore.lastImportCount();
    const size_t routineImports = routineStore.lastImportCount();
    if (eventImports || taskImports || routineImports) {
        char message[96]; snprintf(message, sizeof(message), "%u events, %u tasks and %u routines imported.",
                                   static_cast<unsigned>(eventImports), static_cast<unsigned>(taskImports), static_cast<unsigned>(routineImports));
        shell.notifications().show("ORGANIZER IMPORTED", message);
    } else {
        shell.notifications().show("USB DISK FINISHED", "SD card ejected safely. All organizer data was reloaded.");
    }
}

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("\nPocketPDA v0.2.4");
    const bool essentialHardwareReady = hardware.begin();
    settingsStore.load();
    const bool storageReady = hardware.storage.mount(hardware.spi);
    if (storageReady) {
        calendarStore.load();
        assignmentStore.load();
        habitStore.load();
        taskStore.load();
        noteStore.load();
        packingStore.load();
        routineStore.load();
        timerStore.load();
    }
    shell.begin(hardware, today, transition, capture, calendar, tasks, assignments, packing, routines, notes, clockApp, habits, files, settings);
    usbDisk.setFinishedCallback(usbDiskFinished, nullptr);
    backup.setRestoredCallback(dataRestored, nullptr);
    input.begin(shell);
    power.begin(hardware, settingsStore.powerConfig());
    reminders.begin(calendarStore, taskStore, hardware.rtc, shell);
    timerService.begin(timerStore, hardware.rtc, shell);
    lowBattery.begin(hardware.battery, shell);
    if (calendarStore.lastImportCount() || taskStore.lastImportCount() || routineStore.lastImportCount()) {
        char message[96]; snprintf(message, sizeof(message), "%u events, %u tasks and %u routines imported.",
                                   static_cast<unsigned>(calendarStore.lastImportCount()), static_cast<unsigned>(taskStore.lastImportCount()),
                                   static_cast<unsigned>(routineStore.lastImportCount()));
        shell.notifications().show("ORGANIZER IMPORTED", message);
    } else if (!essentialHardwareReady) shell.notifications().show("HARDWARE WARNING", "One or more essential devices were not detected. See the serial log.");
    else if (!storageReady) shell.notifications().show("STORAGE WARNING", "microSD was not mounted. Organizer data cannot be saved.");
}

void loop() {
    usbDisk.update();
    if (!usbDisk.active()) {
        hardware.update();
        shell.update();
        power.update();
        reminders.update();
        timerService.update();
        lowBattery.update();
    }
    lv_timer_handler();
    if (millis() - lastMemoryLog >= 60000) {
        lastMemoryLog = millis();
        Serial.printf("[PocketPDA] heap=%u min=%u psram=%u\n", ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getFreePsram());
    }
    delay(5);
}
