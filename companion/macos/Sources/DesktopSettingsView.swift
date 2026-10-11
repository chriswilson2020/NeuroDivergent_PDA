import SwiftUI

struct DesktopSettingsView: View {
    @ObservedObject var model: PagerDesktopModel
    @AppStorage("companionAppearance") private var appearance="system"
    @AppStorage("calendarDefaultView") private var calendarView="Week"
    @AppStorage("compactOrganizerRows") private var compact=false
    var body: some View {
        Form {
            Section("Mac companion") {
                Picker("Appearance",selection:$appearance) {
                    Text("Follow macOS").tag("system");Text("Light").tag("light");Text("Dark").tag("dark")
                }
                Toggle("Compact organizer rows",isOn:$compact)
                Text("These preferences apply to this Mac only; they do not change the pager.").font(.caption).foregroundStyle(.secondary)
            }
            Section("Calendar") {
                Picker("Default view",selection:$calendarView) {
                    ForEach(["Day","Week","Month","Agenda"],id:\.self) {Text($0)}
                }
                Text("Weeks run Monday–Sunday. Events use the pager's timezone. Editing a repeating event changes the whole series.")
                    .font(.caption).foregroundStyle(.secondary)
            }
            Section("Attached pager") {
                LabeledContent("Name",value:model.deviceName)
                LabeledContent("Connection",value:model.connected ? "USB Disk Mode" : "Not connected")
                LabeledContent("Timezone",value:model.calendar.timeZone.identifier)
                LabeledContent("Radio configured",value:model.radioEnabled ? "Enabled" : "Disabled or unavailable")
                LabeledContent("Paired contacts",value:String(model.contacts.count))
                Text("Brightness, sleep and other hardware settings are still changed on the pager itself. Its radio pauses during USB Disk Mode.")
                    .font(.caption).foregroundStyle(.secondary)
            }
            Section("About") {
                LabeledContent("Companion version",value:Bundle.main.object(forInfoDictionaryKey:"CFBundleShortVersionString") as? String ?? "Development")
                Text("Calendar transfers, time sync, backups and restore are under Sync & Backup in the sidebar.")
                    .font(.caption).foregroundStyle(.secondary)
            }
        }.formStyle(.grouped)
    }
}
