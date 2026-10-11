#include <Arduino.h>
#include <SD.h>
#include "apps/calendar/CalendarApp.h"
#include "apps/assignments/AssignmentsApp.h"
#include "apps/capture/CaptureApp.h"
#include "apps/clock/ClockApp.h"
#include "apps/files/FilesApp.h"
#include "apps/habits/HabitsApp.h"
#include "apps/notes/NotesApp.h"
#include "apps/messages/MessagesApp.h"
#include "apps/packing/PackingApp.h"
#include "apps/routines/RoutinesApp.h"
#include "apps/settings/SettingsApp.h"
#include "apps/tasks/TasksApp.h"
#include "apps/today/TodayApp.h"
#include "apps/transition/TransitionApp.h"
#include "core/PowerManager.h"
#include "core/SleepCoordinator.h"
#include "core/ReminderService.h"
#include "core/TimerService.h"
#include "core/BackupService.h"
#include "core/LowBatteryService.h"
#include "core/MessagingService.h"
#include "core/GnssTimeService.h"
#include "core/CompanionTimeRequest.h"
#include "core/DesktopBridge.h"
#include <esp_system.h>
#include "data/CalendarStore.h"
#include "data/AssignmentStore.h"
#include "data/HabitStore.h"
#include "data/NoteStore.h"
#include "data/PackingStore.h"
#include "data/RoutineStore.h"
#include "data/SettingsStore.h"
#include "data/PasscodeStore.h"
#include "data/TaskStore.h"
#include "data/TimerStore.h"
#include "data/MessageStore.h"
#include "data/MessagingSettingsStore.h"
#include "hardware/HardwareManager.h"
#include "hardware/InputManager.h"
#include "hardware/UsbDiskService.h"
#include "ui/Shell.h"

static HardwareManager hardware;
static Shell shell;
static InputManager input;
static PowerManager power;
static SleepCoordinator sleepCoordinator;
static SettingsStore settingsStore;
static PasscodeStore passcodes;
static MessagingSettingsStore messagingSettings;
static CalendarStore calendarStore(hardware.storage, hardware.spi);
static AssignmentStore assignmentStore(hardware.storage, hardware.spi);
static HabitStore habitStore(hardware.storage, hardware.spi);
static TaskStore taskStore(hardware.storage, hardware.spi);
static NoteStore noteStore(hardware.storage, hardware.spi);
static PackingStore packingStore(hardware.storage, hardware.spi);
static RoutineStore routineStore(hardware.storage, hardware.spi);
static TimerStore timerStore(hardware.storage, hardware.spi);
static MessageStore messageStore(hardware.storage, hardware.spi);
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
static MessagingService messaging;
static GnssTimeService gnss;
static MessagesApp messages(messageStore, messagingSettings, messaging, shell);
static FilesApp files(hardware.storage, hardware.spi);
static SettingsApp settings(settingsStore, hardware, power, hardware.rtc, shell, usbDisk, backup, gnss);
static ReminderService reminders;
static LowBatteryService lowBattery;
static uint32_t lastMemoryLog = 0;
static uint32_t lastRadioEnergyLog = 0;
static constexpr const char *kBackupRequestPath = "/PocketPDA/commands/backup.request";
static char companionSession[48] = {};
static const char *consumeTimeRequest() {
    SPIBusManager::Guard guard(hardware.spi);
    if (!guard || !SD.exists("/PocketPDA/commands/time.request")) return nullptr;
    File file = SD.open("/PocketPDA/commands/time.request", FILE_READ);
    char text[160];
    const size_t size = file ? file.size() : 0;
    const bool read = file && size <= sizeof(text) && file.readBytes(text, size) == size;
    if (file) file.close();
    // Consume before applying, even if malformed; never replay after another USB exit.
    if (!SD.remove("/PocketPDA/commands/time.request")) return "Time request could not be consumed. Clock unchanged.";
    int64_t epoch = 0;
    bool ok = read && CompanionTimeRequest::parse(text, size, companionSession, epoch);
    if (ok) ok = hardware.rtc.setUtc(static_cast<time_t>(epoch));
    File result = SD.open("/PocketPDA/commands/time.result", FILE_WRITE);
    if (result) {
        result.printf("%s\n%s\n%lld\n", companionSession, ok ? "OK" : "FAILED", static_cast<long long>(epoch));
        result.close();
    }
    return ok ? "Clock set from your Mac. Pager timezone unchanged." : "Time sync failed. Re-enter USB Disk Mode and retry with the updated app.";
}

static bool consumeBackupRequest() {
    if (!hardware.storage.mounted()) return false;
    SPIBusManager::Guard guard(hardware.spi);
    if (!guard || !SD.exists(kBackupRequestPath)) return false;
    return SD.remove(kBackupRequestPath);
}

static void writeJsonString(File &file,const char *value) {
    file.print('"');
    for(const uint8_t *p=reinterpret_cast<const uint8_t*>(value);*p;++p) {
        if(*p=='"'||*p=='\\'){file.print('\\');file.write(*p);}
        else if(*p<32)file.printf("\\u%04x",*p);
        else file.write(*p);
    }
    file.print('"');
}
static void exportDesktopManifest() {
    File file=SD.open("/PocketPDA/commands/desktop.json",FILE_WRITE);
    if(!file)return;
    const auto &config=messagingSettings.value();
    file.print("{\"version\":1,\"deviceName\":");writeJsonString(file,config.deviceName);
    file.printf(",\"deviceID\":\"%012llx\",\"firmware\":\"PocketPDA %s %s\"",static_cast<unsigned long long>(ESP.getEfuseMac()),__DATE__,__TIME__);
    time_t rtcEpoch=0;
    if(hardware.rtc.utcNow(rtcEpoch))file.printf(",\"rtcEpoch\":%lld",static_cast<long long>(rtcEpoch));
    if(hardware.battery.valid())file.printf(",\"batteryPercent\":%u,\"charging\":%s,\"batteryMv\":%u",hardware.battery.percent(),hardware.battery.charging()?"true":"false",hardware.battery.voltageMv());
    file.print(",\"timezone\":");writeJsonString(file,settingsStore.timeSync().timezone==0?"Europe/Amsterdam":"UTC");
    file.printf(",\"radioEnabled\":%s,\"contacts\":[",config.enabled?"true":"false");
    bool first=true;
    for(const auto &contact:config.contacts)if(contact.deviceId) {
        if(!first)file.print(',');first=false;
        file.printf("{\"id\":\"%016llx\",\"name\":",static_cast<unsigned long long>(contact.deviceId));
        writeJsonString(file,contact.name);file.print('}');
    }
    file.print("]}\n");file.close();
}
static size_t consumeDesktopMessages(size_t &failed) {
    failed=0;char names[16][37]{};size_t count=0,queued=0;
    {
        SPIBusManager::Guard guard(hardware.spi);if(!guard)return 0;
        File folder=SD.open("/PocketPDA/messages/desktop-outbox",FILE_READ);
        if(!folder||!folder.isDirectory())return 0;
        while(count<16){File file=folder.openNextFile();if(!file)break;
            const char *name=strrchr(file.path(),'/');name=name?name+1:file.name();
            if(!file.isDirectory()&&DesktopBridge::draftName(name))strlcpy(names[count++],name,sizeof(names[0]));
            file.close();}
        folder.close();
    }
    for(size_t i=0;i<count;++i) {
        char path[100],idPath[104];snprintf(path,sizeof(path),"/PocketPDA/messages/desktop-outbox/%s",names[i]);snprintf(idPath,sizeof(idPath),"%s.id",path);
        DesktopBridge::MessageDraft draft;uint32_t messageId=0;bool valid=false;
        {
            SPIBusManager::Guard guard(hardware.spi);if(!guard){++failed;continue;}
            File file=SD.open(path,FILE_READ);uint8_t data[189];const size_t size=file?file.size():0;
            valid=file&&size<=sizeof(data)&&file.read(data,size)==size&&DesktopBridge::parseMessage(data,size,draft);file.close();
            valid=valid&&messagingSettings.findContact(draft.contact)&&messagingSettings.value().enabled;
            if(valid&&SD.exists(idPath)) {
                File id=SD.open(idPath,FILE_READ);valid=id&&id.size()==4&&id.read(reinterpret_cast<uint8_t*>(&messageId),4)==4&&messageId!=0;id.close();
            } else if(valid) {
                messageId=messagingSettings.nextMessageId();
                File id=SD.open(idPath,FILE_WRITE);valid=id&&messageId&&id.write(reinterpret_cast<const uint8_t*>(&messageId),4)==4;id.flush();id.close();
                File check=SD.open(idPath,FILE_READ);uint32_t stored=0;
                valid=valid&&check&&check.size()==4&&check.read(reinterpret_cast<uint8_t*>(&stored),4)==4&&stored==messageId;check.close();
            }
        }
        // The allocated radio ID is durable before append. If power fails after
        // append but before deleting the request, replay finds the existing
        // outgoing record instead of creating a second message.
        bool ok=valid&&messageStore.findOutgoing(draft.contact,messageId);
        uint32_t local=0;
        if(valid&&!ok)ok=messageStore.addOutgoing(draft.contact,messageId,draft.timestamp,draft.text,local);
        {
            SPIBusManager::Guard guard(hardware.spi);
            if(guard){
                SD.mkdir("/PocketPDA/commands/message-results");char result[112];snprintf(result,sizeof(result),"/PocketPDA/commands/message-results/%s.txt",names[i]);
                File file=SD.open(result,FILE_WRITE);if(file){file.print(ok?"QUEUED\n":"FAILED: check paired contact, radio setting, request integrity and SD space.\n");file.close();}
                if(ok&&SD.remove(path))SD.remove(idPath);
            }
        }
        if(ok)++queued;else ++failed;
    }
    return queued;
}

static void dataRestored(void *) {
    gnss.cancel();
    settingsStore.load();
    hardware.rtc.timezoneChanged();
    gnss.preferencesChanged();
    calendarStore.load();
    assignmentStore.load();
    habitStore.load();
    taskStore.load();
    noteStore.load();
    packingStore.load();
    routineStore.load();
    timerStore.load();
    messageStore.load();
    timerService.reload();
    power.setConfig(settingsStore.powerConfig());
    hardware.setBrightness(settingsStore.value().brightness);
}

static void usbDiskFinished(void *, bool storageReady) {
    messaging.resume();
    if (!storageReady) {
        shell.notifications().show("SD CARD ERROR", "The SD card could not be remounted after USB Disk Mode.");
        return;
    }
    const bool backupRequested = consumeBackupRequest();
    const char *timeResult = consumeTimeRequest();
    calendarStore.load();
    assignmentStore.load();
    habitStore.load();
    taskStore.load();
    noteStore.load();
    packingStore.load();
    routineStore.load();
    timerStore.load();
    messageStore.load();
    size_t desktopMessageFailures=0;
    const size_t desktopMessages=consumeDesktopMessages(desktopMessageFailures);
    shell.setMessageUnread(messageStore.unreadCount());
    timerService.reload();
    const size_t eventImports = calendarStore.lastImportCount();
    const size_t taskImports = taskStore.lastImportCount();
    const size_t routineImports = routineStore.lastImportCount();
    if (backupRequested) {
        if (backup.create()) shell.notifications().show("BACKUP READY", "Requested by the companion app. Re-enter USB Disk Mode to download it.");
        else shell.notifications().show("BACKUP FAILED", backup.lastError());
    } else if (timeResult) {
        shell.notifications().show("MAC TIME SYNC", timeResult);
    } else if (desktopMessages || desktopMessageFailures) {
        char message[112];snprintf(message,sizeof(message),"%u queued for radio, %u pending/error. Delivery is shown in Messages.",static_cast<unsigned>(desktopMessages),static_cast<unsigned>(desktopMessageFailures));
        shell.notifications().show("DESKTOP MESSAGES",message);
    } else if (eventImports || taskImports || routineImports) {
        char message[96]; snprintf(message, sizeof(message), "%u events, %u tasks and %u routines imported.",
                                   static_cast<unsigned>(eventImports), static_cast<unsigned>(taskImports), static_cast<unsigned>(routineImports));
        shell.notifications().show("ORGANIZER IMPORTED", message);
    } else {
        shell.notifications().show("USB DISK FINISHED", "SD card ejected safely. All organizer data was reloaded.");
    }
}

static void usbDiskStarted(void *) {
    gnss.cancel(); messaging.suspend();
    companionSession[0] = 0;
    const bool settingsExported = backup.exportSettingsSnapshot();
    SPIBusManager::Guard guard(hardware.spi);
    if (!guard) return;
    SD.mkdir("/PocketPDA/commands");
    exportDesktopManifest();
    SD.remove("/PocketPDA/commands/backup-settings.session");
    char session[48];
    snprintf(session, sizeof(session), "%012llx-%08lx%08lx",
             static_cast<unsigned long long>(ESP.getEfuseMac()),
             static_cast<unsigned long>(esp_random()), static_cast<unsigned long>(esp_random()));
    // Remove stale session before publishing a fresh one. Failure leaves sync unavailable.
    SD.remove("/PocketPDA/commands/time.session");
    File file = SD.open("/PocketPDA/commands/time.session", FILE_WRITE);
    if (file) {
        const size_t written = file.printf("%s\n", session);
        file.close();
        if (written == strlen(session) + 1) strlcpy(companionSession, session, sizeof(companionSession));
    }
    if (settingsExported && companionSession[0]) {
        File marker = SD.open("/PocketPDA/commands/backup-settings.session", FILE_WRITE);
        if (marker) { marker.printf("%s\n", companionSession); marker.close(); }
    }
}
static void openMessageConversation(void *context, uint64_t contactId) {
    shell.openApp("messages");
    static_cast<MessagesApp *>(context)->openConversation(contactId);
}

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("\nPocketPDA v0.3.3");
    settingsStore.load();
    passcodes.load();
    messagingSettings.load();
    const bool essentialHardwareReady = hardware.begin(messagingSettings.value().enabled);
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
        messageStore.load();
    }
    shell.begin(hardware, today, transition, capture, calendar, tasks, assignments, packing, routines, notes, clockApp, habits, messages, files, settings);
    shell.beginLock(passcodes);
    shell.setMessageUnread(messageStore.unreadCount());
    messaging.begin(hardware, messageStore, messagingSettings, hardware.rtc, shell);
    messaging.setOpenConversationCallback(openMessageConversation, &messages);
    usbDisk.setStartedCallback(usbDiskStarted, nullptr);
    usbDisk.setAccessGuard([](void *){return !shell.locked();},nullptr);
    usbDisk.setFinishedCallback(usbDiskFinished, nullptr);
    backup.setRestoredCallback(dataRestored, nullptr);
    input.begin(shell);
    power.begin(hardware, settingsStore.powerConfig());
    gnss.begin(hardware,power,settingsStore);
    reminders.begin(calendarStore, taskStore, hardware.rtc, shell);
    timerService.begin(timerStore, hardware.rtc, shell);
    lowBattery.begin(hardware.battery, shell);
    sleepCoordinator.begin(hardware,power,timerService,reminders,messaging);
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
        messaging.update();
        gnss.update(false);
    } else {
        gnss.update(true);
    }
    lv_timer_handler();
    if(!usbDisk.active()) shell.ensureFocus();
    sleepCoordinator.setLockDeadlineUs(shell.nextLockDeadlineUs());
    if (millis() - lastMemoryLog >= 60000) {
        lastMemoryLog = millis();
        Serial.printf("[PocketPDA] heap=%u min=%u psram=%u\n", ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getFreePsram());
    }
    if (millis() - lastRadioEnergyLog >= 600000) {
        lastRadioEnergyLog = millis();
        const auto radioStats = messaging.stats();
        Serial.printf("[PocketPDA][radio-energy] uptime=%lus rx=%llums tx=%llums sleep=%llums packets-rx=%lu packets-tx=%lu battery=%u%% %umV %dmA usb=%u\n",
                      static_cast<unsigned long>(millis()/1000),radioStats.receiveMs,radioStats.transmitMs,radioStats.sleepMs,
                      static_cast<unsigned long>(radioStats.packetsReceived),static_cast<unsigned long>(radioStats.packetsSent),
                      hardware.battery.percent(),hardware.battery.voltageMv(),hardware.battery.currentMa(),hardware.battery.usbPresent());
    }
    // GNSS cancellation must have cut the rail before processor sleep. An
    // expander readback failure is retried, never treated as permission to sleep.
    if(gnss.active()&&power.state()==PowerState::SLEEP){gnss.cancel();delay(5);}
    else sleepCoordinator.idle(usbDisk.active());
}
