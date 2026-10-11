import Foundation

@main
struct PagerDesktopTests {
    @MainActor static func main() throws {
        let fixtureRoot=URL(fileURLWithPath:CommandLine.arguments[1])
        func rejects(_ operation: () throws -> Void) {do {try operation();fatalError("Expected rejection")} catch {}}
        for schema in PagerSchema.all {
            let original=try Data(contentsOf:fixtureRoot.appendingPathComponent(schema.id+".dat"))
            let records=try PagerStoreCodec.decode(original,schema:schema)
            assert(records.count==1 && records[0].id==41)
            let roundTrip=try PagerStoreCodec.encode(records,schema:schema);assert(roundTrip==original)
            var edited=records[0];try edited.setText(schema.titleOffset,schema.titleSize,"Edited ë")
            let editedData=try PagerStoreCodec.encode([edited],schema:schema)
            let loaded=try PagerStoreCodec.decode(editedData,schema:schema)[0];assert(loaded.title(schema)=="Edited ë")
            for i in 0..<schema.size where !(schema.titleOffset..<(schema.titleOffset+schema.titleSize)).contains(i) {assert(edited.bytes[i]==records[0].bytes[i])}
            var corrupt=original;corrupt[12]^=1;rejects {_ = try PagerStoreCodec.decode(corrupt,schema:schema)}
            var version=original;version[4]=2;rejects {_ = try PagerStoreCodec.decode(version,schema:schema)}
            rejects {try edited.setText(schema.titleOffset,schema.titleSize,String(repeating:"ë",count:schema.titleSize))}
        }
        var calendar=Calendar(identifier:.gregorian);calendar.timeZone=TimeZone(identifier:"Europe/Amsterdam")!
        let lessonStart=calendar.date(from:DateComponents(year:2026,month:10,day:12,hour:9))!
        let imported=CompanionEvent(id:"outlook-test",title:"Maths",location:"e0066",start:lessonStart,end:lessonStart.addingTimeInterval(3600),reminder:5)
        let firstPlan=try CompanionCalendar.plan([imported,imported],existing:[],calendar:calendar)
        assert(firstPlan.added == 1 && firstPlan.duplicates == 1)
        let again=try CompanionCalendar.plan([imported],existing:firstPlan.records,calendar:calendar,origins:firstPlan.origins)
        assert(again.added == 0 && again.duplicates == 1)
        let changed=CompanionEvent(id:imported.id,title:"Maths updated",location:imported.location,start:imported.start,end:imported.end,reminder:5)
        let refreshed=try CompanionCalendar.plan([changed],existing:firstPlan.records,calendar:calendar,origins:firstPlan.origins)
        assert(refreshed.updated == 1 && refreshed.records.count == 1 && refreshed.records[0].id == firstPlan.records[0].id)
        var locallyEdited=firstPlan.records[0];try locallyEdited.setText(15,40,"My local edit")
        let conflict=try CompanionCalendar.plan([changed],existing:[locallyEdited],calendar:calendar,origins:firstPlan.origins)
        assert(conflict.updated == 0 && conflict.records == [locallyEdited] && conflict.warnings.count == 1)
        let ics="BEGIN:VCALENDAR\r\nBEGIN:VEVENT\r\nUID:123\r\nDTSTART;TZID=Europe/Amsterdam:20261012T090000\r\nDTEND;TZID=Europe/Amsterdam:20261012T100000\r\nSUMMARY:Maths\\, class\r\nEND:VEVENT\r\nEND:VCALENDAR\r\n"
        let parsed=try CompanionCalendar.readICS(Data(ics.utf8),calendar:calendar)
        assert(parsed.count == 1 && parsed[0].title == "Maths, class" && parsed[0].start == lessonStart)
        rejects {_ = try CompanionCalendar.readICS(Data(ics.replacingOccurrences(of:"UID:123",with:"UID:123\r\nRRULE:FREQ=WEEKLY").utf8),calendar:calendar)}
        let diagnostics=CompanionDiagnostics.parse("epoch,uptime_ms,soc,usb,cpu_sleep_ms,cpu_awake_ms,packets_rx,packets_tx,sleep_errors\n1000,30000,95,0,20000,10000,2,4,0\nmalformed\n1001,31000,NaN,0,20000,10000,2,4,0\n")
        assert(diagnostics.count == 1 && diagnostics[0].soc == 95)
        let event=try PagerStoreCodec.decode(Data(contentsOf:fixtureRoot.appendingPathComponent("calendar.dat")),schema:.named("calendar"))[0]
        let monday=calendar.date(from:DateComponents(year:2026,month:10,day:12))!,sunday=calendar.date(byAdding:.day,value:-1,to:monday)!
        assert(PagerStoreCodec.occurs(event,on:monday,calendar:calendar));assert(!PagerStoreCodec.occurs(event,on:sunday,calendar:calendar))
        assert(DesktopCalendarLayout.weekStart(sunday,calendar:calendar) == calendar.date(byAdding:.day,value:-6,to:sunday))
        let month=DesktopCalendarLayout.monthDays(monday,calendar:calendar)
        assert(month.count == 42 && calendar.component(.weekday,from:month[0]) == 2)
        assert(calendar.component(.day,from:month[0]) == 28 && calendar.component(.month,from:month[0]) == 9)
        // Calendar arithmetic, not fixed seconds, keeps headers correct across DST.
        for i in 1..<month.count {assert(calendar.dateComponents([.day],from:month[i-1],to:month[i]).day == 1)}
        func timed(_ id: UInt64,_ start: Int,_ end: Int) -> PagerRecord {
            var r=event;r.setInteger(0,4,id);r.setInteger(8,1,UInt64(start/60));r.setInteger(9,1,UInt64(start%60));r.setInteger(10,1,UInt64(end/60));r.setInteger(11,1,UInt64(end%60));return r
        }
        let placed=DesktopCalendarLayout.placements([timed(1,540,600),timed(2,570,630),timed(3,600,660),timed(4,660,720)])
        assert(placed.map(\.lane) == [0,1,0,0] && placed.map(\.lanes) == [2,2,2,1])
        assert(DesktopCalendarLayout.placements([]).isEmpty)
        let buttons=[CGSize(width:140,height:28),CGSize(width:170,height:28),CGSize(width:160,height:32)]
        for width:CGFloat in [300,400,560,800,1200] {
            let flow=DesktopFlowMetrics.arrange(sizes:buttons,width:width,spacing:10)
            for index in buttons.indices {
                assert(flow.points[index].x >= 0 && flow.points[index].x+buttons[index].width <= width)
                assert(flow.points[index].y+buttons[index].height <= flow.size.height)
                if index > 0 {
                    let sameRow=flow.points[index].y == flow.points[index-1].y
                    assert(!sameRow || flow.points[index].x >= flow.points[index-1].x+buttons[index-1].width+10)
                }
            }
        }
        assert(DesktopFlowMetrics.arrange(sizes:buttons,width:300,spacing:10).size.height == 108)
        assert(DesktopFlowMetrics.arrange(sizes:[],width:400,spacing:10).size.height == 0)
        assert(!DesktopPaneMetrics.usesColumns(580) && DesktopPaneMetrics.usesColumns(800))
        for width:CGFloat in [680,800,1000,1300,1800] {
            let master=DesktopPaneMetrics.masterWidth(width),detail=width-master-1
            assert(master >= 260 && master <= 340 && detail >= 400)
            assert(master+detail+1 == width)
        }
        // Month grids use only their actual rows and include the entire month.
        for year in [2026,2028] {
            for monthNumber in 1...12 {
                let target=calendar.date(from:DateComponents(year:year,month:monthNumber,day:15))!
                let visible=DesktopCalendarLayout.visibleMonthDays(target,calendar:calendar)
                assert([28,35,42].contains(visible.count))
                assert(calendar.component(.weekday,from:visible.first!) == 2)
                assert(calendar.component(.weekday,from:visible.last!) == 1)
                let actual=visible.filter {calendar.component(.month,from:$0) == monthNumber && calendar.component(.year,from:$0) == year}
                assert(actual.count == calendar.range(of:.day,in:.month,for:target)!.count)
            }
        }
        let fm=FileManager.default,volume=fm.temporaryDirectory.appendingPathComponent("PocketPDA-desktop-tests-\(UUID().uuidString)")
        let root=volume.appendingPathComponent("PocketPDA")
        defer {try? fm.removeItem(at:volume)}
        try fm.createDirectory(at:root.appendingPathComponent("commands"),withIntermediateDirectories:true)
        let session=root.appendingPathComponent("commands/time.session");try Data("test-session\n".utf8).write(to:session)
        CompanionVault.testDirectory=volume.appendingPathComponent("MacBackups")
        try Data("test-session\n".utf8).write(to:root.appendingPathComponent("commands/backup-settings.session"))
        var snapshot=Data([0x31,0x42,0x44,0x50,1,0,2,0])
        for path in ["@settings","@timesync"] {
            let body=Data([1,2,3,4]);var entry=Data(path.utf8);entry.append(Data(repeating:0,count:88-entry.count))
            CompanionBackup.append32(UInt32(body.count),to:&entry);CompanionBackup.append32(CompanionBackup.checksum(body),to:&entry)
            snapshot.append(entry);snapshot.append(body)
        }
        try snapshot.write(to:root.appendingPathComponent("commands/backup-settings.ppb"))
        for schema in PagerSchema.all {
            let target=root.appendingPathComponent(schema.path)
            try fm.createDirectory(at:target.deletingLastPathComponent(),withIntermediateDirectories:true)
            try Data(contentsOf:fixtureRoot.appendingPathComponent(schema.id+".dat")).write(to:target)
        }
        let model=PagerDesktopModel();model.connect(root);assert(model.connected && model.errors.isEmpty)
        let calendarBeforeBackupFailure=try Data(contentsOf:root.appendingPathComponent("calendar/events.dat"))
        try Data("stale-snapshot\n".utf8).write(to:root.appendingPathComponent("commands/backup-settings.session"))
        rejects {try model.save(model.items("calendar")[0],schema:.named("calendar"))}
        let calendarAfterBackupFailure=try Data(contentsOf:root.appendingPathComponent("calendar/events.dat"))
        assert(calendarBeforeBackupFailure == calendarAfterBackupFailure)
        try Data("test-session\n".utf8).write(to:root.appendingPathComponent("commands/backup-settings.session"))
        let calendarSchema=PagerSchema.named("calendar")
        var changedEvent=model.items("calendar")[0]
        let eventID=changedEvent.id
        try changedEvent.setText(15,40,"Edited calendar event")
        try model.save(changedEvent,schema:calendarSchema)
        let recovery=try CompanionVault.archives(model.lease!)
        assert(recovery.count == 1)
        _ = try CompanionBackup.validate(recovery[0])
        var otherEvent=try model.create(calendarSchema);try otherEvent.setText(15,40,"Keep this event")
        try model.save(otherEvent,schema:calendarSchema)
        try model.remove(changedEvent,schema:calendarSchema)
        let afterDelete=try PagerStoreCodec.decode(Data(contentsOf:root.appendingPathComponent(calendarSchema.path)),schema:calendarSchema)
        assert(afterDelete.count == 1 && afterDelete[0].id == otherEvent.id && afterDelete[0].bytes == otherEvent.bytes)
        assert(!afterDelete.contains {$0.id == eventID})
        let tasks=PagerSchema.named("tasks"),original=model.items("tasks")[0]
        var task=original;try task.setText(12,48,"Updated task")
        try model.save(task,schema:tasks)
        let stored=try PagerStoreCodec.decode(Data(contentsOf:root.appendingPathComponent(tasks.path)),schema:tasks)
        assert(stored[0].title(tasks)=="Updated task")
        let undoFiles=Array(fm.enumerator(atPath:root.appendingPathComponent("desktop-undo").path)!).compactMap {$0 as? String}
        assert(undoFiles.contains {$0.hasSuffix("tasks.dat")})
        // External changes and staged imports are never silently replaced.
        let taskURL=root.appendingPathComponent(tasks.path);try Data("external".utf8).write(to:taskURL)
        rejects {try model.save(task,schema:tasks)}
        try PagerStoreCodec.encode([task],schema:tasks).write(to:taskURL);model.connect(root)
        try Data("title,notes,due,priority,reminder,repeat\n".utf8).write(to:root.appendingPathComponent("tasks/import.csv"))
        rejects {try model.save(task,schema:tasks)}
        try fm.removeItem(at:root.appendingPathComponent("tasks/import.csv"))
        // Dates, recurrence and habits preserve their non-editor history fields.
        var recurring=model.items("tasks")[0];recurring.setInteger(6,1,3);recurring.setInteger(8,2,2026);recurring.setInteger(10,1,10);recurring.setInteger(11,1,9)
        try model.save(recurring,schema:tasks);try model.completeTask(recurring)
        assert(model.items("tasks")[0].integer(11)==12 && model.items("tasks")[0].integer(4)==0)
        let habit=model.items("habits")[0];var renamed=habit;try renamed.setText(16,32,"Hydrate")
        try model.save(renamed,schema:.named("habits"));assert(model.items("habits")[0].integer(12,2)==7)
        // A reconnected pager cannot receive an old editor's save.
        try Data("new-session\n".utf8).write(to:session);rejects {try model.save(task,schema:tasks)}
        try Data("new-session\n".utf8).write(to:root.appendingPathComponent("commands/backup-settings.session"))
        model.connect(root);model.contacts=[.init(id:"1122334455667788",name:"Test contact")];model.radioEnabled=true
        try model.queueMessage(contact:"1122334455667788",text:"Hello from the desktop")
        let outbox=root.appendingPathComponent("messages/desktop-outbox")
        let draft=try fm.contentsOfDirectory(at:outbox,includingPropertiesForKeys:nil).first!
        try Data(contentsOf:draft).write(to:fixtureRoot.appendingPathComponent("swift-draft.msg"))
        assert(model.items("messages").count==1) // sent history is never rewritten by compose
        rejects {try model.queueMessage(contact:"ffffffffffffffff",text:"Not paired")}
        let lease=try PagerWriteLease(root:root)
        try Data("new-session\n".utf8).write(to:root.appendingPathComponent("commands/backup-settings.session"))
        let bodyPath="notes/00000029.txt",indexPath="notes/index.dat"
        let index=try Data(contentsOf:root.appendingPathComponent(indexPath)),body=Data("Desktop note body".utf8)
        try lease.saveNote(body:body,bodyPath:bodyPath,originalBody:nil,index:index,indexPath:indexPath,originalIndex:index)
        let savedBody=try Data(contentsOf:root.appendingPathComponent(bodyPath));assert(savedBody == body)
        rejects {try lease.saveNote(body:Data("Changed".utf8),bodyPath:bodyPath,originalBody:body,index:index,indexPath:indexPath,originalIndex:Data())}
        let unchangedBody=try Data(contentsOf:root.appendingPathComponent(bodyPath));assert(unchangedBody == body)
        let source=volume.appendingPathComponent("source.txt");try body.write(to:source)
        try lease.importFile(source,path:"files/example.txt")
        let importedBody=try Data(contentsOf:root.appendingPathComponent("files/example.txt"));assert(importedBody == body)
        rejects {try lease.importFile(source,path:"files/example.txt")}
        let link=volume.appendingPathComponent("linked.txt");try fm.createSymbolicLink(at:link,withDestinationURL:source)
        rejects {try lease.importFile(link,path:"files/linked.txt")}
        print("Desktop editor tests passed: firmware fixtures, round trips, field preservation, safe writes, conflicts, recurrence, habits and message queue")
    }
}
