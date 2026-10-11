import Foundation

struct CompanionEvent: Identifiable {
    let id: String, title: String, location: String, start: Date, end: Date, reminder: Int
}
struct CalendarImportPlan {
    var records: [PagerRecord]
    var added=0, duplicates=0
    var updated=0
    var origins:[String:Data]=[:]
    var warnings: [String]=[]
}
enum CompanionCalendar {
    // Full record identity rather than title-only matching. Never removes local events.
    static func plan(_ events: [CompanionEvent], existing: [PagerRecord], calendar: Calendar,origins:[String:Data]=[:]) throws -> CalendarImportPlan {
        var plan=CalendarImportPlan(records:existing,origins:origins)
        let schema=PagerSchema.named("calendar")
        func key(_ r: PagerRecord) -> Data {Data(r.bytes.dropFirst(4))}
        var known=Set(existing.map(key)), next=existing.map(\.id).max() ?? 0
        for event in events.sorted(by:{$0.start < $1.start}) {
            guard calendar.isDate(event.start,inSameDayAs:event.end), event.end > event.start else {
                plan.warnings.append("Skipped multi-day or invalid event: \(event.title)");continue
            }
            guard !event.title.isEmpty,event.title.utf8.count < 40,event.location.utf8.count < 20 else {
                plan.warnings.append("Shorten title/location to fit pager: \(event.title)");continue
            }
            let d=calendar.dateComponents([.year,.month,.day,.hour,.minute],from:event.start)
            let end=calendar.dateComponents([.hour,.minute],from:event.end)
            guard (2020...2099).contains(d.year ?? 0) else {plan.warnings.append("Unsupported date: \(event.title)");continue}
            var record=PagerRecord(bytes:Data(repeating:0,count:schema.size))
            record.setInteger(4,2,UInt64(d.year!));record.setInteger(6,1,UInt64(d.month!));record.setInteger(7,1,UInt64(d.day!))
            record.setInteger(8,1,UInt64(d.hour!));record.setInteger(9,1,UInt64(d.minute!))
            record.setInteger(10,1,UInt64(end.hour!));record.setInteger(11,1,UInt64(end.minute!))
            record.setInteger(12,2,UInt64(min(1440,max(0,event.reminder))))
            try record.setText(15,40,event.title);try record.setText(55,20,event.location)
            if let prior=origins[event.id],prior.count == schema.size {
                let old=PagerRecord(bytes:prior)
                guard let index=plan.records.firstIndex(where:{$0.id == old.id}),plan.records[index].bytes == prior else {
                    plan.warnings.append("Kept locally edited/deleted event: \(event.title)");continue
                }
                if key(record) == key(old) {plan.duplicates += 1;continue}
                record.setInteger(0,4,UInt64(old.id));plan.records[index]=record;plan.origins[event.id]=record.bytes
                known.remove(key(old));known.insert(key(record));plan.updated += 1;continue
            }
            guard known.insert(key(record)).inserted else {plan.duplicates += 1;continue}
            guard next < UInt32.max else {throw PagerDataError.invalid("Calendar IDs exhausted.")}
            next += 1;record.setInteger(0,4,UInt64(next));plan.records.append(record);plan.origins[event.id]=record.bytes;plan.added += 1
        }
        _ = try PagerStoreCodec.encode(plan.records,schema:schema)
        return plan
    }
    // Conservative offline ICS reader: unsupported recurrence is rejected rather
    // than silently turning a series into one appointment. EventKit expands series.
    static func readICS(_ data: Data, calendar: Calendar) throws -> [CompanionEvent] {
        guard data.count <= 10_000_000,let text=String(data:data,encoding:.utf8),text.contains("BEGIN:VCALENDAR") else {throw PagerDataError.invalid("Not a UTF-8 iCalendar file (maximum 10 MB).")}
        let lines=text.replacingOccurrences(of:"\r\n",with:"\n").replacingOccurrences(of:"\n ",with:"").replacingOccurrences(of:"\n\t",with:"").split(separator:"\n").map(String.init)
        var fields: [String:(String,String)]=[:], result:[CompanionEvent]=[], inside=false
        func unescape(_ value:String)->String {value.replacingOccurrences(of:"\\n",with:" ").replacingOccurrences(of:"\\N",with:" ").replacingOccurrences(of:"\\,",with:",").replacingOccurrences(of:"\\;",with:";").replacingOccurrences(of:"\\\\",with:"\\")}
        func date(_ key:String) throws -> Date {
            guard let (head,value)=fields[key],!head.contains("VALUE=DATE"),value.count >= 15 else {throw PagerDataError.invalid("ICS contains all-day or missing times. Import through macOS Calendar for these calendars.")}
            var zone=calendar.timeZone
            if value.hasSuffix("Z") {zone=TimeZone(secondsFromGMT:0)!}
            else if let range=head.range(of:"TZID=") {
                let name=String(head[range.upperBound...]).split(separator:";")[0].trimmingCharacters(in:CharacterSet(charactersIn:"\""))
                guard let parsed=TimeZone(identifier:name) else {throw PagerDataError.invalid("Unknown ICS timezone: \(name)")};zone=parsed
            }
            let formatter=DateFormatter();formatter.locale=Locale(identifier:"en_US_POSIX");formatter.timeZone=zone
            formatter.dateFormat=value.hasSuffix("Z") ? "yyyyMMdd'T'HHmmss'Z'" : "yyyyMMdd'T'HHmmss";formatter.isLenient=false
            guard let date=formatter.date(from:value) else {throw PagerDataError.invalid("Invalid ICS date: \(value)")};return date
        }
        var alarm=false
        for line in lines {
            if line == "BEGIN:VEVENT" {guard !inside else {throw PagerDataError.invalid("Nested ICS event.")};inside=true;fields=[:];continue}
            if line == "BEGIN:VALARM" {alarm=true;continue};if line == "END:VALARM" {alarm=false;continue}
            if line == "END:VEVENT" {
                guard inside,fields["RRULE"] == nil,fields["RDATE"] == nil,fields["EXDATE"] == nil,fields["RECURRENCE-ID"] == nil else {throw PagerDataError.invalid("Recurring ICS events require expansion. Add this file to macOS Calendar, then preview that calendar here.")}
                result.append(.init(id:fields["UID"]?.1 ?? UUID().uuidString,title:unescape(fields["SUMMARY"]?.1 ?? "Untitled"),location:unescape(fields["LOCATION"]?.1 ?? ""),start:try date("DTSTART"),end:try date("DTEND"),reminder:5));inside=false;continue
            }
            guard inside,!alarm,let colon=line.firstIndex(of:":") else {continue}
            let head=String(line[..<colon]),key=String(head.split(separator:";")[0]);fields[key]=(head,String(line[line.index(after:colon)...]))
        }
        guard !inside,!result.isEmpty else {throw PagerDataError.invalid("Incomplete ICS file or no timed events.")}
        return result
    }
}
