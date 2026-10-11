import AppKit
import SwiftUI

struct CompanionCalendarPreview:View {
    @ObservedObject var companion:CompanionModel
    @ObservedObject var desktop:PagerDesktopModel
    @Environment(\.dismiss) private var dismiss
    @State private var plan:CalendarImportPlan?
    @State private var issue:String?
    @State private var sourceSession:String?
    @State private var baseline:[PagerRecord]=[]
    private var originsURL:URL? {desktop.lease.map {CompanionVault.directory($0).appendingPathComponent("calendar-origins.json")}}
    var body:some View {
        VStack(alignment:.leading,spacing:16) {
            Text("Calendar import preview").font(.title2.bold())
            if let plan {
                Text("\(plan.added) additions · \(plan.updated) updates · \(plan.duplicates) duplicates · \(plan.warnings.count) warnings")
                Text("One-way to pager; Outlook is never modified. Previously imported events are updated only if unmodified locally. Local edits/deletions are kept. Events removed from Outlook are retained here; remove them with Bulk Tools.").font(.caption).foregroundStyle(.secondary)
                ScrollView {
                    LazyVStack(alignment:.leading,spacing:8) {
                        ForEach(plan.warnings,id:\.self) {Text($0).foregroundStyle(.orange)}
                        ForEach(plan.records.filter {record in !desktop.items("calendar").contains(record)}) {record in
                            Text("\(record.title(.named("calendar"))) · \(record.date(4,6,7,calendar:desktop.calendar)?.formatted(date:.abbreviated,time:.omitted) ?? "") · \(String(format:"%02d:%02d",Int(record.integer(8)),Int(record.integer(9))))")
                        }
                    }.frame(maxWidth:.infinity,alignment:.leading)
                }.frame(minHeight:180)
            }
            if let issue {Text(issue).foregroundStyle(.red)}
            HStack {
                Button("Cancel") {dismiss()};Spacer()
                Button("Back Up and Save to Pager") {
                    do {
                        guard desktop.lease?.session == sourceSession,desktop.items("calendar") == baseline else {throw PagerDataError.invalid("The device or calendar changed after preview. Cancel and preview again.")}
                        if let plan {try desktop.replaceCalendar(plan.records);if let originsURL {try JSONEncoder().encode(plan.origins).write(to:originsURL,options:.atomic)};companion.detail="Calendar saved. Eject to apply."};dismiss()
                    }
                    catch {issue=error.localizedDescription}
                }.buttonStyle(.borderedProminent).disabled(plan == nil || (plan!.added+plan!.updated == 0))
            }
        }.padding(24).frame(width:620,height:480)
        .onAppear {do {
            sourceSession=desktop.lease?.session;baseline=desktop.items("calendar")
            let origins=try originsURL.flatMap {FileManager.default.fileExists(atPath:$0.path) ? try JSONDecoder().decode([String:Data].self,from:Data(contentsOf:$0)) : nil} ?? [:]
            plan=try CompanionCalendar.plan(companion.calendarPreview,existing:desktop.items("calendar"),calendar:desktop.calendar,origins:origins)
        } catch {issue=error.localizedDescription}}
    }
}

struct CompanionCalendarToolsView:View {
    @ObservedObject var model:PagerDesktopModel
    @Environment(\.dismiss) private var dismiss
    @State private var selected=Set<UInt32>()
    @State private var start=Date()
    @State private var end=Date()
    @State private var sourceWeek=Date()
    @State private var targetWeek=Date().addingTimeInterval(7*86400)
    @State private var issue:String?
    @State private var confirming=false
    @State private var preview:[PagerRecord]?
    @State private var previewLabel=""
    @State private var sourceSession:String?
    @State private var baseline:[PagerRecord]=[]
    private let schema=PagerSchema.named("calendar")
    var body:some View {
        VStack(alignment:.leading,spacing:14) {
            Text("Bulk Calendar Tools").font(.title2.bold())
            Text("Select stored events. Selecting a repeating event deletes the whole series. Date range selection matches the series' starting date, not each occurrence.").font(.caption).foregroundStyle(.secondary)
            DesktopFlow {
                DatePicker("From",selection:$start,displayedComponents:.date).fixedSize()
                DatePicker("To",selection:$end,displayedComponents:.date).fixedSize()
                Button("Select Range") {selected=Set(model.items("calendar").filter {r in guard let d=r.date(4,6,7,calendar:model.calendar) else {return false};return model.calendar.startOfDay(for:d) >= model.calendar.startOfDay(for:start) && model.calendar.startOfDay(for:d) <= model.calendar.startOfDay(for:end)}.map(\.id))}
                Button("Clear") {selected=[]}
            }
            ScrollView {
                LazyVStack(alignment:.leading,spacing:8) {
                    ForEach(model.items("calendar")) {r in
                        Toggle(isOn:Binding(get:{selected.contains(r.id)},set:{if $0 {selected.insert(r.id)} else {selected.remove(r.id)}})) {
                            Text("\(r.date(4,6,7,calendar:model.calendar)?.formatted(date:.abbreviated,time:.omitted) ?? "") · \(r.title(schema))\(r.integer(14) != 0 ? " · series" : "")")
                        }
                    }
                }
            }.frame(minHeight:150)
            DesktopFlow {
                Button("Delete \(selected.count) Selected…",role:.destructive) {confirming=true}.disabled(selected.isEmpty)
                Button("Import iCalendar (.ics)…") {importICS()}
            }
            Divider()
            Text("Copy a school week").font(.headline)
            DesktopFlow {
                DatePicker("Source week",selection:$sourceWeek,displayedComponents:.date).fixedSize()
                DatePicker("Destination week",selection:$targetWeek,displayedComponents:.date).fixedSize()
                Button("Preview Copy…") {copyWeek()}
            }
            if let preview {
                Text(previewLabel).font(.callout)
                Button("Back Up and Apply Preview") {perform {try model.replaceCalendar(preview);self.preview=nil}}
            }
            if let issue {Text(issue).foregroundStyle(.red)}
            HStack {Spacer();Button("Done") {dismiss()}}
        }.padding(24).frame(width:700,height:620)
        .onAppear {sourceSession=model.lease?.session;baseline=model.items("calendar")}
        .confirmationDialog("Delete \(selected.count) stored events/series?",isPresented:$confirming,titleVisibility:.visible) {
            Button("Back Up and Delete",role:.destructive) {perform {try model.replaceCalendar(model.items("calendar").filter {!selected.contains($0.id)});selected=[];preview=nil}}
        }
    }
    private func perform(_ action:() throws->Void) {do {
        guard model.lease?.session == sourceSession,model.items("calendar") == baseline else {throw PagerDataError.invalid("Device or calendar changed. Close and reopen Bulk Tools.")}
        try action();baseline=model.items("calendar");issue=nil
    } catch {issue=error.localizedDescription}}
    private func importICS() {
        let panel=NSOpenPanel();panel.allowedContentTypes=[.calendarEvent,.data]
        guard panel.runModal() == .OK,let url=panel.url else {return}
        perform {
            let events=try CompanionCalendar.readICS(Data(contentsOf:url),calendar:model.calendar)
            let plan=try CompanionCalendar.plan(events,existing:model.items("calendar"),calendar:model.calendar)
            preview=plan.records;previewLabel="ICS: \(plan.added) additions, \(plan.duplicates) duplicates.\n"+plan.warnings.joined(separator:"\n")
        }
    }
    private func copyWeek() {
        perform {
            let source=DesktopCalendarLayout.weekStart(sourceWeek,calendar:model.calendar),target=DesktopCalendarLayout.weekStart(targetWeek,calendar:model.calendar)
            var events:[CompanionEvent]=[]
            for offset in 0..<7 {
                let day=model.calendar.date(byAdding:.day,value:offset,to:source)!,destination=model.calendar.date(byAdding:.day,value:offset,to:target)!
                for r in model.events(on:day) {
                    let start=model.calendar.date(bySettingHour:Int(r.integer(8)),minute:Int(r.integer(9)),second:0,of:destination)!
                    let end=model.calendar.date(bySettingHour:Int(r.integer(10)),minute:Int(r.integer(11)),second:0,of:destination)!
                    events.append(.init(id:UUID().uuidString,title:r.title(schema),location:r.text(55,20),start:start,end:end,reminder:Int(r.integer(12,2))))
                }
            }
            let plan=try CompanionCalendar.plan(events,existing:model.items("calendar"),calendar:model.calendar)
            preview=plan.records;previewLabel="Copy: \(plan.added) one-off lessons, \(plan.duplicates) duplicates skipped.\n"+plan.warnings.joined(separator:"\n")
        }
    }
}
