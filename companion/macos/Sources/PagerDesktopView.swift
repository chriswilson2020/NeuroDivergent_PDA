import AppKit
import SwiftUI

private struct DesktopAppItem: Identifiable {
    let id: String, title: String, icon: String
    static let all: [DesktopAppItem] = [
        .init(id:"today",title:"Today",icon:"sun.max"), .init(id:"transition",title:"Transition",icon:"hourglass"),
        .init(id:"capture",title:"Quick Capture",icon:"square.and.pencil")
    ] + PagerSchema.all.map { .init(id:$0.id,title:$0.title,icon:$0.icon) } + [
        .init(id:"files",title:"Files",icon:"folder"), .init(id:"settings",title:"Settings",icon:"gearshape")]
}

struct ContentView: View {
    @StateObject private var companion=CompanionModel()
    @StateObject private var desktop=PagerDesktopModel()
    @State private var selected: String?="today"
    @AppStorage("companionAppearance") private var appearance="system"
    var body: some View {
        NavigationSplitView {
            List(selection:$selected) {
                Section {
                    VStack(alignment:.leading,spacing:6) {
                        Label(desktop.deviceName,systemImage:"iphone.gen1").font(.headline)
                        Label(companion.connected ? "USB Disk connected" : "Not connected",systemImage:companion.connected ? "circle.fill" : "circle")
                            .font(.caption).foregroundStyle(companion.connected ? .green : .secondary)
                    }.padding(.vertical,10)
                }
                Section("PAGER APPS") {
                    ForEach(DesktopAppItem.all) { item in
                        Label(item.title,systemImage:item.icon).tag(item.id)
                    }
                }
                Section("COMPANION") {
                    Label("Device",systemImage:"iphone").tag("device")
                    Label("Sync & Backup",systemImage:"arrow.triangle.2.circlepath").tag("sync")
                    Label("Power & Radio",systemImage:"chart.xyaxis.line").tag("diagnostics")
                }
            }
            .listStyle(.sidebar).navigationSplitViewColumnWidth(min:180,ideal:210,max:260)
        } detail: {
            GeometryReader {bounds in
            Group {
                if !companion.connected {
                    VStack {
                    ContentUnavailableView("Connect your PocketPDA",systemImage:"cable.connector",description:Text("Plug in the pager and choose Settings → USB DISK. Your data stays on its microSD card."))
                    Button("Choose Device…") { companion.chooseDevice() }.padding(.bottom,24)
                    }
                } else {
                    detail.environment(\.timeZone,desktop.calendar.timeZone).environment(\.calendar,desktop.calendar).disabled(companion.busy)
                }
            }.frame(width:bounds.size.width,height:bounds.size.height,alignment:.topLeading)
                .clipped()
            }
            .safeAreaInset(edge:.bottom,spacing:0) {
                VStack(spacing:0) {
                Divider()
                HStack(spacing:12) {
                    Image(systemName:companion.connected ? "externaldrive.connected.to.line.below" : "cable.connector.slash")
                    Text(desktop.status).font(.caption).lineLimit(1).truncationMode(.tail)
                        .frame(maxWidth:.infinity,alignment:.leading)
                    if companion.busy { ProgressView().controlSize(.small) }
                    if desktop.hasChanges && companion.connected {
                        Button("Eject to Apply") {companion.eject()}.disabled(companion.busy)
                    }
                    Text("v\(Bundle.main.object(forInfoDictionaryKey:"CFBundleShortVersionString") as? String ?? "—")")
                        .font(.caption).foregroundStyle(.secondary).fixedSize()
                }.padding(.horizontal,14).padding(.vertical,10).background(.bar)
                }.fixedSize(horizontal:false,vertical:true)
            }
            .navigationTitle(DesktopAppItem.all.first { $0.id == selected }?.title ?? (selected == "device" ? "Device" : selected == "diagnostics" ? "Power & Radio" : "Sync & Backup"))
            .toolbar {
                ToolbarItemGroup {
                    Button { desktop.connect(companion.pocketRoot) } label: { Label("Reload",systemImage:"arrow.clockwise") }
                        .disabled(!companion.connected || companion.busy)
                    Button { companion.requestBackup() } label: { Label("Back Up",systemImage:"externaldrive.badge.checkmark") }
                        .disabled(!companion.connected || companion.busy)
                    Button { companion.eject() } label: { Label("Eject Pager",systemImage:"eject") }
                        .disabled(!companion.connected || companion.busy)
                }
            }
        }
        .onChange(of:companion.pocketRoot,initial:true) { _,root in desktop.connect(root) }
        .sheet(isPresented:$companion.showingCalendarPreview) {CompanionCalendarPreview(companion:companion,desktop:desktop)}
        .frame(minWidth:820,minHeight:560)
        .preferredColorScheme(appearance == "light" ? .light : appearance == "dark" ? .dark : nil)
    }
    @ViewBuilder private var detail: some View {
        switch selected {
        case "sync": CompanionToolsView(companion:companion,desktop:desktop)
        case "device": CompanionDashboardView(model:desktop)
        case "diagnostics": CompanionDiagnosticsView(model:desktop)
        case "settings": DesktopSettingsView(model:desktop)
        case "today": DesktopTodayView(model:desktop,transition:false)
        case "transition": DesktopTodayView(model:desktop,transition:true)
        case "capture": DesktopCaptureView(model:desktop)
        case "calendar": DesktopCalendarView(model:desktop)
        case "files": DesktopFilesView(model:desktop)
        default:
            if let schema=PagerSchema.all.first(where:{$0.id == selected}) {
                DesktopRecordsView(model:desktop,schema:schema).id(schema.id)
            }
        }
    }
}

private struct EditItem: Identifiable {
    var record: PagerRecord
    var id: UInt32 { record.id }
}

struct DesktopRecordsView: View {
    @ObservedObject var model: PagerDesktopModel
    let schema: PagerSchema
    @State private var search=""
    @State private var selection: UInt32?
    @State private var editing: EditItem?
    @State private var error: String?
    @State private var deleting: PagerRecord?
    @State private var selectedDay=Date()
    @State private var showAll=false
    @State private var composing=false
    @AppStorage("compactOrganizerRows") private var compact=false
    init(model:PagerDesktopModel,schema:PagerSchema,selectedRecord:UInt32?=nil) {
        self.model=model;self.schema=schema
        _selection=State(initialValue:selectedRecord)
    }
    var filtered: [PagerRecord] {
        let source=schema.id == "calendar" && !showAll ? model.events(on:selectedDay) : model.items(schema.id)
        return source.filter { search.isEmpty || $0.title(schema).localizedCaseInsensitiveContains(search) }
    }
    var chosen: PagerRecord? { model.items(schema.id).first { $0.id == selection } }
    var body: some View {
        GeometryReader {bounds in
        VStack(spacing:0) {
            HStack {
                TextField("Search \(schema.title.lowercased())",text:$search).textFieldStyle(.roundedBorder)
                Text("\(model.items(schema.id).count) items").foregroundStyle(.secondary).font(.caption)
                if schema.id == "messages" {
                    Button("Compose…") { composing=true }.disabled(model.contacts.isEmpty || !model.radioEnabled)
                } else {
                    Button { perform { editing=EditItem(record:try model.create(schema)) } } label:{ Label("New",systemImage:"plus") }.keyboardShortcut("n")
                }
            }.padding(16).fixedSize(horizontal:false,vertical:true)
            if let issue=model.errors[schema.id] {
                ContentUnavailableView("Could not read \(schema.title)",systemImage:"exclamationmark.triangle",description:Text(issue))
            } else {
                if schema.id == "calendar" {
                    HStack {
                        DatePicker("Agenda",selection:$selectedDay,displayedComponents:.date).datePickerStyle(.stepperField)
                        Button("Today") { selectedDay=Date() }
                        Toggle("All stored events",isOn:$showAll).toggleStyle(.checkbox)
                        Spacer()
                        Text(model.calendar.timeZone.identifier).font(.caption).foregroundStyle(.secondary)
                    }.padding(.horizontal).padding(.bottom,12)
                }
                if schema.id == "habits" { DesktopPetView(model:model) }
                if filtered.isEmpty {
                    ContentUnavailableView(search.isEmpty ? "No \(schema.title.lowercased()) yet" : "No matching items",systemImage:schema.icon,
                        description:Text(search.isEmpty ? (schema.readOnly ? "Received and sent messages appear here." : "Choose New to add your first item.") : "Try a different search."))
                        .frame(maxWidth:.infinity,maxHeight:.infinity)
                } else if DesktopPaneMetrics.usesColumns(bounds.size.width) {
                    HStack(spacing:0) {
                        recordList.frame(width:DesktopPaneMetrics.masterWidth(bounds.size.width)).clipped()
                        Divider().frame(width:1)
                        detailPane.frame(width:max(0,bounds.size.width-DesktopPaneMetrics.masterWidth(bounds.size.width)-1)).clipped()
                    }.frame(maxWidth:.infinity,maxHeight:.infinity)
                } else if chosen != nil {
                    VStack(alignment:.leading,spacing:0) {
                        Button {selection=nil} label:{Label("All \(schema.title)",systemImage:"chevron.left")}.padding(.horizontal,16).padding(.bottom,10)
                        detailPane
                    }.frame(maxWidth:.infinity,maxHeight:.infinity)
                } else {recordList.frame(maxWidth:.infinity,maxHeight:.infinity)}
            }
        }.frame(width:bounds.size.width,height:bounds.size.height,alignment:.topLeading)
        }
        .disabled(!model.connected)
        .sheet(item:$editing) { item in PagerRecordEditor(model:model,schema:schema,record:item.record) }
        .sheet(isPresented:$composing) { DesktopMessageComposer(model:model) }
        .alert("Unable to save",isPresented:Binding(get:{error != nil},set:{if !$0 {error=nil}})) { Button("OK"){} } message:{Text(error ?? "")}
        .confirmationDialog("Delete this item from the pager?",isPresented:Binding(get:{deleting != nil},set:{if !$0 {deleting=nil}}),titleVisibility:.visible) {
            Button("Delete",role:.destructive) { if let item=deleting { perform { try model.remove(item,schema:schema); selection=nil }; deleting=nil } }
            Button("Cancel",role:.cancel) { deleting=nil }
        }
    }
    private var recordList: some View {
        ScrollView {
            LazyVStack(alignment:.leading,spacing:4) {
                        ForEach(filtered) { record in
                            Button {selection=record.id} label:{
                            VStack(alignment:.leading,spacing:5) {
                                Text(record.title(schema)).font(.body.weight(.medium)).lineLimit(3)
                                    .fixedSize(horizontal:false,vertical:true)
                                Text(subtitle(record)).font(.caption).foregroundStyle(.secondary).lineLimit(3)
                                    .fixedSize(horizontal:false,vertical:true)
                            }.frame(maxWidth:.infinity,alignment:.leading).padding(compact ? 8 : 12)
                                .background(selection == record.id ? Color.accentColor.opacity(0.16) : Color.clear,in:RoundedRectangle(cornerRadius:8))
                                .contentShape(Rectangle())
                            }.buttonStyle(.plain).frame(maxWidth:.infinity,alignment:.leading)
                                .accessibilityIdentifier("pager-record-\(schema.id)-\(record.id)")
                                .contextMenu {
                                    if !schema.readOnly {
                                        Button("Edit…") { editing=EditItem(record:record) }
                                        Button("Delete…",role:.destructive) { deleting=record }
                                    }
                                }
                        }
            }.padding(10).frame(maxWidth:.infinity,alignment:.topLeading)
        }.frame(maxWidth:.infinity,maxHeight:.infinity,alignment:.topLeading)
            .background(Color(nsColor:.controlBackgroundColor))
            .onKeyPress(.downArrow) {moveSelection(1);return .handled}
            .onKeyPress(.upArrow) {moveSelection(-1);return .handled}
    }
    private func moveSelection(_ offset:Int) {
        guard !filtered.isEmpty else {return}
        let index=selection.flatMap {id in filtered.firstIndex(where:{$0.id == id})} ?? (offset > 0 ? -1 : filtered.count)
        selection=filtered[max(0,min(filtered.count-1,index+offset))].id
    }
    @ViewBuilder private var detailPane: some View {
                    if let chosen {
                        ScrollView {
                            VStack(alignment:.leading,spacing:18) {
                                Text(chosen.title(schema)).font(.title2.bold()).textSelection(.enabled)
                                Text(subtitle(chosen)).foregroundStyle(.secondary)
                                recordDetails(chosen)
                                if !schema.readOnly {
                                    DesktopFlow {
                                        Button("Edit…") { editing=EditItem(record:chosen) }.buttonStyle(.borderedProminent)
                                        if schema.id == "habits" {
                                            Button(chosen.integer(4,4) == model.dayKey(Date()) ? "Undo today's check-in" : "Complete today") { perform { try model.toggleHabit(chosen) } }
                                        }
                                        if schema.id == "tasks" {
                                            Button(chosen.integer(4) == 0 ? "Complete task" : "Mark incomplete") { perform { try model.completeTask(chosen) } }
                                        }
                                        if schema.id == "assignments" {
                                            Button("Complete next step") { perform { try model.completeAssignment(chosen) } }.disabled(chosen.integer(4) != 0)
                                        }
                                        Button("Delete…",role:.destructive) { deleting=chosen }
                                    }
                                }
                            }.padding(24).frame(maxWidth:.infinity,alignment:.leading)
                        }.frame(maxWidth:.infinity,maxHeight:.infinity)
                    } else {
                        ContentUnavailableView(filtered.isEmpty ? "No items" : "Select an item",systemImage:schema.icon,
                            description:Text(schema.readOnly ? "Message history is read-only. Compose a new message to queue it for the pager." : "View details or create a new item."))
                            .frame(maxWidth:.infinity,maxHeight:.infinity)
                    }
    }
    private func perform(_ operation: () throws -> Void) { do { try operation() } catch { self.error=error.localizedDescription } }
    private func subtitle(_ record: PagerRecord) -> String {
        switch schema.id {
        case "calendar":
            let date=showAll ? String(format:"%04d-%02d-%02d · ",Int(record.integer(4,2)),Int(record.integer(6)),Int(record.integer(7))) : ""
            return date + String(format:"%02d:%02d–%02d:%02d · %@",Int(record.integer(8)),Int(record.integer(9)),Int(record.integer(10)),Int(record.integer(11)),record.text(55,20))
        case "tasks": return record.integer(4) != 0 ? "Completed" : (record.integer(5) == 2 ? "High priority" : "To do")
        case "assignments": return record.integer(4) != 0 ? "Completed" : "Step \(min(record.integer(7)+1,record.integer(6))) of \(record.integer(6)) · \(record.integer(8,2)) min"
        case "habits": return "\(record.integer(12,2))-day streak" + (record.integer(4,4) == model.dayKey(Date()) ? " · Done today" : "")
        case "messages":
            let delivery=["Queued","Sending","Delivered","Failed"][min(3,Int(record.integer(21)))]
            return "\(record.integer(20) == 0 ? "From" : "To") \(model.contactName(record)) · \(record.integer(20) == 0 ? "Received" : delivery)"
        case "clock": return record.integer(4) == 0 ? "\(record.integer(8,2)) minute countdown" : String(format:"Daily alarm %02d:%02d",Int(record.integer(10)),Int(record.integer(11)))
        case "packing": return "Matches: \(record.text(5,24)) · \(record.integer(4)) items"
        case "routines": return "\(record.integer(4)) steps"
        default: return "On your pager"
        }
    }
    @ViewBuilder private func recordDetails(_ record: PagerRecord) -> some View {
        if schema.id == "notes" {
            let data=try? model.noteBody(record.id)
            Text(String(decoding:data ?? Data(),as:UTF8.self)).textSelection(.enabled)
        } else if schema.id == "messages" {
            Text(Date(timeIntervalSince1970:Double(record.integer(16,4))),style:.date).font(.caption)
            Text("History is not edited or retransmitted. Radio transmission resumes after ejecting USB Disk Mode.").font(.caption).foregroundStyle(.secondary)
        } else {
            ForEach(schema.fields) { field in
                if field.label != "Title" && field.label != "Name" && field.label != "Habit" {
                    VStack(alignment:.leading,spacing:4) {
                        Text(field.label).font(.caption).foregroundStyle(.secondary)
                        Text(PagerFieldFormatting.value(record,field:field,calendar:model.calendar)).textSelection(.enabled)
                    }
                }
            }
            if schema.id == "routines", let field=schema.fields.last { DesktopRoutineSteps(record:record,field:field).id(record.id) }
        }
    }
}

enum PagerFieldFormatting {
    static func value(_ record: PagerRecord, field: PagerField, calendar: Calendar) -> String {
        switch field.kind {
        case .text(let size): return record.text(field.offset,size)
        case .number(let width,_,_): return String(record.integer(field.offset,width))
        case .toggle: return record.integer(field.offset) == 0 ? "No" : "Yes"
        case .choice(let choices): return choices.first {$0.0 == Int(record.integer(field.offset))}?.1 ?? "Custom (\(record.integer(field.offset)))"
        case .date(let month,let day):
            guard let date=record.date(field.offset,month,day,calendar:calendar) else { return "No date" }
            let formatter=DateFormatter(); formatter.dateStyle = .medium; formatter.calendar=calendar; formatter.timeZone=calendar.timeZone; return formatter.string(from:date)
        case .time: return String(format:"%02d:%02d",Int(record.integer(field.offset)),Int(record.integer(field.offset+1)))
        case .lines(let count,let width,let countOffset): return (0..<min(count,Int(record.integer(countOffset)))).map { record.text(field.offset+$0*width,width) }.joined(separator:"\n")
        }
    }
}

struct PagerRecordEditor: View {
    @ObservedObject var model: PagerDesktopModel
    let schema: PagerSchema
    @State var record: PagerRecord
    @State private var bodyText=""
    @State private var originalBody: Data?
    @State private var bodyLoaded=false
    @State private var fieldErrors: [String:String]=[:]
    @State private var error: String?
    @State private var confirmingDelete=false
    @Environment(\.dismiss) private var dismiss
    private var storedRecord: PagerRecord? {model.items(schema.id).first {$0.id == record.id}}
    private var isSeries: Bool {schema.id == "calendar" && (storedRecord?.integer(14) ?? 0) != 0}
    private var deleteLabel: String {schema.id == "calendar" ? (isSeries ? "Delete Series…" : "Delete Event…") : "Delete…"}
    var body: some View {
        VStack(alignment:.leading,spacing:16) {
            Text("Edit \(schema.title)").font(.title2.bold())
            Text("Saved directly to the attached pager. Eject afterwards to reload its apps.").font(.caption).foregroundStyle(.secondary)
            Form {
                ForEach(schema.fields) { field in RecordFieldEditor(record:$record,field:field,calendar:model.calendar,validation:$fieldErrors) }
                if schema.id == "notes" {
                    Section("Note") { TextEditor(text:$bodyText).frame(minHeight:180).font(.body) }
                }
            }.formStyle(.grouped)
            if let error { Text(error).foregroundStyle(.red).font(.callout).textSelection(.enabled) }
            HStack {
                Button("Cancel") { dismiss() }.keyboardShortcut(.cancelAction)
                if storedRecord != nil && !schema.readOnly {
                    Button(deleteLabel,role:.destructive) {confirmingDelete=true}.disabled(!model.connected)
                }
                Spacer()
                Button("Save to Pager") {
                    do { try model.save(record,schema:schema,body:schema.id == "notes" ? bodyText : nil,originalBody:originalBody); dismiss() }
                    catch { self.error=error.localizedDescription }
                }.keyboardShortcut(.defaultAction).disabled(!model.connected || !fieldErrors.isEmpty || (schema.id == "notes" && !bodyLoaded))
            }
        }.padding(24).frame(width:610,height:schema.id == "notes" ? 680 : 620)
        .interactiveDismissDisabled()
        .confirmationDialog(isSeries ? "Delete the entire repeating series?" : "Delete this item from the pager?",isPresented:$confirmingDelete,titleVisibility:.visible) {
            Button(isSeries ? "Delete Series" : "Delete",role:.destructive) {
                do {
                    if let storedRecord {try model.remove(storedRecord,schema:schema)}
                    dismiss()
                } catch {self.error=error.localizedDescription}
            }
            Button("Keep Item",role:.cancel) {}
        } message: {
            Text(isSeries ? "This removes every occurrence of this stored event. Unsaved edits will be discarded." : "The item will be removed from the pager. Unsaved edits will be discarded.")
        }
        .onAppear {
            if schema.id == "notes" {
                do {
                    originalBody=try model.noteBody(record.id)
                    guard let value=String(data:originalBody ?? Data(),encoding:.utf8) else {throw PagerDataError.invalid("This note is not valid UTF-8. Export the original before editing.")}
                    bodyText=value;bodyLoaded=true
                }
                catch { self.error=error.localizedDescription }
            }
        }
    }
}

struct RecordFieldEditor: View {
    @Binding var record: PagerRecord
    let field: PagerField, calendar: Calendar
    @Binding var validation: [String:String]
    @State private var text=""
    @State private var error: String?
    var body: some View {
        VStack(alignment:.leading,spacing:4) {
            control
            if let error { Text(error).font(.caption).foregroundStyle(.red) }
        }.onAppear { updateText() }.onChange(of:error) {_,value in validation[field.id]=value}
    }
    @ViewBuilder private var control: some View {
        switch field.kind {
        case .text(let width):
            TextField(field.label,text:Binding(get:{text},set:{new in text=new; change { try record.setText(field.offset,width,new) }}))
            Text("\(text.utf8.count)/\(width-1) bytes").font(.caption2).foregroundStyle(.secondary)
        case .number(let width,let low,let high):
            Stepper("\(field.label): \(record.integer(field.offset,width))",value:Binding(get:{Int(record.integer(field.offset,width))},set:{record.setInteger(field.offset,width,UInt64($0))}),in:low...high)
        case .toggle:
            Toggle(field.label,isOn:Binding(get:{record.integer(field.offset) != 0},set:{record.setInteger(field.offset,1,$0 ? 1 : 0)}))
        case .choice(let options):
            Picker(field.label,selection:Binding(get:{Int(record.integer(field.offset))},set:{record.setInteger(field.offset,1,UInt64($0))})) {
                if !options.contains(where:{$0.0 == Int(record.integer(field.offset))}) { Text("Custom pattern (preserved)").tag(Int(record.integer(field.offset))) }
                ForEach(options,id:\.0) { Text($0.1).tag($0.0) }
            }
        case .date(let month,let day):
            HStack {
                Toggle(field.label,isOn:Binding(get:{record.integer(field.offset,2) != 0},set:{enabled in
                    if enabled { setDate(Date(),month:month,day:day) } else { record.setInteger(field.offset,2,0);record.setInteger(month,1,0);record.setInteger(day,1,0) }
                })).toggleStyle(.checkbox)
                DatePicker("",selection:Binding(get:{record.date(field.offset,month,day,calendar:calendar) ?? Date()},set:{setDate($0,month:month,day:day)}),displayedComponents:.date)
                    .labelsHidden().disabled(record.integer(field.offset,2) == 0).environment(\.timeZone,calendar.timeZone)
            }
        case .time:
            DatePicker(field.label,selection:Binding(get:{calendar.date(bySettingHour:Int(record.integer(field.offset)),minute:Int(record.integer(field.offset+1)),second:0,of:Date()) ?? Date()},set:{date in
                record.setInteger(field.offset,1,UInt64(calendar.component(.hour,from:date))); record.setInteger(field.offset+1,1,UInt64(calendar.component(.minute,from:date)))
            }),displayedComponents:.hourAndMinute).environment(\.timeZone,calendar.timeZone)
        case .lines(let count,let width,let countOffset):
            Text(field.label)
            TextEditor(text:Binding(get:{text},set:{new in
                text=new
                change {
                    let lines=new.split(separator:"\n",omittingEmptySubsequences:true).map(String.init)
                    guard lines.count <= count else { throw PagerDataError.invalid("At most \(count) lines.") }
                    var updated=record
                    for i in 0..<count { try updated.setText(field.offset+i*width,width,i < lines.count ? lines[i] : "") }
                    updated.setInteger(countOffset,1,UInt64(lines.count))
                    if countOffset == 6 { updated.setInteger(7,1,UInt64(max(0,min(Int(updated.integer(7)),lines.count-1)))) }
                    record=updated
                }
            })).frame(minHeight:120)
            Text("Up to \(count) lines, \(width-1) UTF-8 bytes per line.").font(.caption2).foregroundStyle(.secondary)
        }
    }
    private func updateText() {
        switch field.kind {
        case .text(let width): text=record.text(field.offset,width)
        case .lines: text=PagerFieldFormatting.value(record,field:field,calendar:calendar)
        default: break
        }
    }
    private func change(_ operation: () throws -> Void) { do {try operation();error=nil} catch {self.error=error.localizedDescription} }
    private func setDate(_ date: Date,month:Int,day:Int) {
        let d=calendar.dateComponents([.year,.month,.day],from:date)
        record.setInteger(field.offset,2,UInt64(d.year!));record.setInteger(month,1,UInt64(d.month!));record.setInteger(day,1,UInt64(d.day!))
    }
}

struct DesktopRoutineSteps: View {
    let record: PagerRecord, field: PagerField
    @State private var step=0
    var body: some View {
        if case .lines(let count,let width,let countOffset)=field.kind {
            let total=min(count,Int(record.integer(countOffset)))
            if total > 0 {
                GroupBox("Walk through this routine on your Mac") {
                    VStack(alignment:.leading,spacing:12) {
                        ProgressView(value:Double(min(step,total)),total:Double(total))
                        Text(step < total ? record.text(field.offset+step*width,width) : "All done!").font(.title3)
                        Button(step < total ? "Done · Next step" : "Start again") { step=step < total ? step+1 : 0 }
                    }.padding(8)
                }.id(record.id)
            }
        }
    }
}

struct DesktopPetView: View {
    @ObservedObject var model: PagerDesktopModel
    var body: some View {
        let habits=model.items("habits"), completed=habits.filter {$0.integer(4,4) == model.dayKey(Date())}.count
        HStack(spacing:20) {
            TimelineView(.animation(minimumInterval:0.1)) { timeline in
                Text(completed == habits.count && !habits.isEmpty ? "🥳" : completed > 0 ? "🐣" : "🐥")
                    .font(.system(size:52)).offset(y:sin(timeline.date.timeIntervalSince1970*2)*(completed > 0 ? 4 : 1))
            }.frame(width:80,height:76)
            VStack(alignment:.leading,spacing:6) {
                Text(completed == habits.count && !habits.isEmpty ? "Mochi is proud of you!" : "One small step at a time.").font(.headline)
                Text("\(completed) of \(habits.count) habits completed today").foregroundStyle(.secondary)
                ProgressView(value:Double(completed),total:Double(max(1,habits.count)))
            }
            Spacer()
        }.padding(.horizontal,24).padding(.bottom,14)
    }
}

struct DesktopTodayView: View {
    @ObservedObject var model: PagerDesktopModel
    let transition: Bool
    var body: some View {
        TimelineView(.periodic(from:Date(),by:1)) { context in
            let events=model.events(on:context.date)
            let minutes=model.calendar.component(.hour,from:context.date)*60+model.calendar.component(.minute,from:context.date)
            let upcoming=events.first { Int($0.integer(8)*60+$0.integer(9)) >= minutes }
            ScrollView {
                VStack(alignment:.leading,spacing:24) {
                    Text(context.date.formatted(.dateTime.weekday(.wide).day().month(.wide))).font(.largeTitle.bold())
                    Text("\(model.deviceName)'s desktop").foregroundStyle(.secondary)
                    if let next=upcoming {
                        GroupBox(transition ? "Next transition" : "Coming up") {
                            VStack(alignment:.leading,spacing:12) {
                                Text(next.text(15,40)).font(.title.bold())
                                Text(next.text(55,20)).font(.title3).foregroundStyle(.secondary)
                                let target=model.calendar.date(bySettingHour:Int(next.integer(8)),minute:Int(next.integer(9)),second:0,of:context.date)!
                                let seconds=max(0,Int(target.timeIntervalSince(context.date)))
                                Text(String(format:"%02d:%02d",seconds/60,seconds%60)).font(.system(size:transition ? 70 : 40,weight:.medium,design:.rounded)).monospacedDigit()
                                Text(seconds <= 60 ? "Time to go" : seconds <= 300 ? "Pack up and get ready" : "You've got time").foregroundStyle(.secondary)
                            }.padding(18).frame(maxWidth:.infinity,alignment:.leading)
                        }
                    } else { Text("No more lessons or appointments today.").font(.title2) }
                    GroupBox("Today's agenda") {
                        VStack(alignment:.leading,spacing:12) {
                            ForEach(events) { event in
                                HStack {
                                    Text(String(format:"%02d:%02d",Int(event.integer(8)),Int(event.integer(9)))).monospacedDigit()
                                    Text(event.text(15,40)).fontWeight(.medium).frame(maxWidth:.infinity,alignment:.leading).fixedSize(horizontal:false,vertical:true)
                                    Text(event.text(55,20)).foregroundStyle(.secondary).lineLimit(2).frame(maxWidth:90,alignment:.trailing)
                                }
                            }
                            if events.isEmpty { Text("Nothing scheduled.").foregroundStyle(.secondary) }
                        }.padding(10)
                    }
                    GroupBox("Next actions") {
                        VStack(alignment:.leading,spacing:12) {
                            ForEach(model.items("tasks").filter {$0.integer(4) == 0}.prefix(8)) { Text("○  \($0.text(12,48))") }
                            if model.items("tasks").allSatisfy({$0.integer(4) != 0}) { Text("No open tasks.").foregroundStyle(.secondary) }
                        }.padding(10).frame(maxWidth:.infinity,alignment:.leading)
                    }
                    Text("The pager's radio is paused in USB Disk Mode. These countdowns run on the Mac while it is attached.").font(.caption).foregroundStyle(.secondary)
                }.padding(24).frame(maxWidth:.infinity,alignment:.topLeading)
            }
        }
    }
}

struct DesktopCaptureView: View {
    @ObservedObject var model: PagerDesktopModel
    @State private var text=""
    @State private var type="tasks"
    @State private var result=""
    var body: some View {
        ScrollView {
        VStack(alignment:.leading,spacing:20) {
            Text("Get it out of your head.").font(.largeTitle.bold())
            Text("Capture now. Organize it later.").foregroundStyle(.secondary)
            Picker("Save as",selection:$type) { Text("Task").tag("tasks"); Text("Note").tag("notes") }.pickerStyle(.segmented).frame(maxWidth:260)
            TextEditor(text:$text).font(.title3).frame(height:220).border(.quaternary)
            Button("Capture to Pager") {
                do {
                    let schema=PagerSchema.named(type); var record=try model.create(schema)
                    let title=String(text.split(separator:"\n").first ?? "")
                    try record.setText(schema.titleOffset,schema.titleSize,title)
                    if type == "tasks" {try record.setText(60,96,text.split(separator:"\n").dropFirst().joined(separator:"\n"))}
                    try model.save(record,schema:schema,body:type == "notes" ? text : nil)
                    text=""; result="Captured."
                } catch {result=error.localizedDescription}
            }.buttonStyle(.borderedProminent).disabled(text.trimmingCharacters(in:.whitespacesAndNewlines).isEmpty || !model.connected)
            Text(result).foregroundStyle(.secondary)
        }.padding(24).frame(maxWidth:.infinity,alignment:.topLeading)
        }
    }
}

struct DesktopMessageComposer: View {
    @ObservedObject var model: PagerDesktopModel
    @State private var contact=""
    @State private var text=""
    @State private var error=""
    @Environment(\.dismiss) private var dismiss
    var body: some View {
        VStack(alignment:.leading,spacing:16) {
            Text("New message").font(.title2.bold())
            Picker("To",selection:$contact) { Text("Choose contact").tag(""); ForEach(model.contacts) {Text($0.name).tag($0.id)} }
            TextEditor(text:$text).frame(height:160)
            Text("\(text.utf8.count)/160 bytes. The pager queues this for radio transmission after you eject. Queued does not mean delivered.").font(.caption).foregroundStyle(.secondary)
            if !error.isEmpty { Text(error).foregroundStyle(.red) }
            HStack {
                Button("Cancel") {dismiss()}; Spacer()
                Button("Queue for Send after Eject") {
                    do {try model.queueMessage(contact:contact,text:text);dismiss()} catch {self.error=error.localizedDescription}
                }.buttonStyle(.borderedProminent).disabled(contact.isEmpty || text.isEmpty || text.utf8.count > 160 || !model.connected)
            }
        }.padding(24).frame(width:540).interactiveDismissDisabled()
    }
}

struct DesktopFilesView: View {
    @ObservedObject var model: PagerDesktopModel
    @State private var files: [URL]=[]
    @State private var selected: URL?
    @State private var preview=""
    @State private var error=""
    var body: some View {
        GeometryReader {bounds in
        VStack(alignment:.leading,spacing:16) {
            Text("Pager files").font(.title.bold())
            Text("Your files folder—not internal organizer databases. Import and export files without changing calendars or messages.").foregroundStyle(.secondary)
            DesktopFlow {
                Button("Import File…") { transfer(importing:true) }
                Button("Export Selected…") {transfer(importing:false)}.disabled(selected == nil)
                Button("Open Folder in Finder") {if let lease=model.lease {NSWorkspace.shared.open(lease.root.appendingPathComponent("files"))}}
            }
            if files.isEmpty {
                ContentUnavailableView("No files yet",systemImage:"folder",description:Text("Use Import File to add a file to your pager."))
                    .frame(maxWidth:.infinity,maxHeight:.infinity)
            } else if DesktopPaneMetrics.usesColumns(bounds.size.width-40) {
                let available=bounds.size.width-40
                HStack(spacing:0) {
                    fileList.frame(width:DesktopPaneMetrics.masterWidth(available)).clipped()
                    Divider().frame(width:1)
                    filePreview.frame(width:max(0,available-DesktopPaneMetrics.masterWidth(available)-1)).clipped()
                }.frame(maxWidth:.infinity,maxHeight:.infinity)
            } else if selected != nil {
                Button {selected=nil} label:{Label("All Files",systemImage:"chevron.left")}
                filePreview
            } else {fileList.frame(maxWidth:.infinity,maxHeight:.infinity)}
            if !error.isEmpty {Text(error).foregroundStyle(.red).fixedSize(horizontal:false,vertical:true)}
        }.padding(20).frame(width:bounds.size.width,height:bounds.size.height,alignment:.topLeading)
        }.onAppear {reload()}.onChange(of:selected) {_,url in
            preview=""
            if let url, let size=try? url.resourceValues(forKeys:[.fileSizeKey]).fileSize, size <= 1_048_576, let data=try? Data(contentsOf:url) { preview=String(data:data,encoding:.utf8) ?? "Binary file. Use Export Selected to save a copy." }
            else if url != nil {preview="File is too large to preview."}
        }
    }
    private var fileList: some View {
        ScrollView {
            LazyVStack(alignment:.leading,spacing:4) {
                ForEach(files,id:\.self) {url in
                    Button {selected=url} label:{
                        Label(url.lastPathComponent,systemImage:"doc")
                            .lineLimit(3).fixedSize(horizontal:false,vertical:true)
                            .frame(maxWidth:.infinity,alignment:.leading).padding(12)
                            .background(selected == url ? Color.accentColor.opacity(0.16) : .clear,in:RoundedRectangle(cornerRadius:8))
                            .contentShape(Rectangle())
                    }.buttonStyle(.plain)
                }
            }.padding(10).frame(maxWidth:.infinity,alignment:.topLeading)
        }.frame(maxWidth:.infinity,maxHeight:.infinity).background(Color(nsColor:.controlBackgroundColor))
    }
    private var filePreview: some View {
        ScrollView {Text(selected == nil ? "Select a file to preview it." : preview).textSelection(.enabled).fixedSize(horizontal:false,vertical:true).frame(maxWidth:.infinity,alignment:.leading).padding()}
            .frame(minWidth:0,maxWidth:.infinity,maxHeight:.infinity)
    }
    private func reload() {
        guard let lease=model.lease else {return}
        files=((try? FileManager.default.contentsOfDirectory(at:lease.root.appendingPathComponent("files"),includingPropertiesForKeys:[.isRegularFileKey,.isSymbolicLinkKey],options:.skipsHiddenFiles)) ?? [])
            .filter {url in let values=try? url.resourceValues(forKeys:[.isRegularFileKey,.isSymbolicLinkKey]);return values?.isRegularFile == true && values?.isSymbolicLink != true}.sorted {$0.lastPathComponent < $1.lastPathComponent}
    }
    private func transfer(importing: Bool) {
        do {
            guard let lease=model.lease else {return}; try lease.check()
            if importing {
                let panel=NSOpenPanel();panel.canChooseDirectories=false
                guard panel.runModal() == .OK,let source=panel.url else {return}
                let path="files/\(source.lastPathComponent)"
                let target=lease.root.appendingPathComponent(path)
                guard !FileManager.default.fileExists(atPath:target.path) else {throw PagerDataError.invalid("A file with that name already exists. Rename it before importing.")}
                try lease.importFile(source,path:path);reload()
            } else if let selected {
                let panel=NSSavePanel();panel.nameFieldStringValue=selected.lastPathComponent
                guard panel.runModal() == .OK,let destination=panel.url else {return}
                let fm=FileManager.default,temporary=destination.deletingLastPathComponent().appendingPathComponent(".PocketPDA-export-\(UUID().uuidString)")
                defer {try? fm.removeItem(at:temporary)}
                try fm.copyItem(at:selected,to:temporary)
                if fm.fileExists(atPath:destination.path) {_ = try fm.replaceItemAt(destination,withItemAt:temporary)}
                else {try fm.moveItem(at:temporary,to:destination)}
            }
        } catch {self.error=error.localizedDescription}
    }
}
