import Foundation

enum DesktopCalendarLayout {
    static func weekStart(_ date: Date, calendar: Calendar) -> Date {
        let day=calendar.startOfDay(for:date)
        return calendar.date(byAdding:.day,value:-((calendar.component(.weekday,from:day)+5)%7),to:day)!
    }
    static func monthDays(_ date: Date, calendar: Calendar) -> [Date] {
        let first=calendar.dateInterval(of:.month,for:date)!.start
        let start=weekStart(first,calendar:calendar)
        return (0..<42).map {calendar.date(byAdding:.day,value:$0,to:start)!}
    }
    static func visibleMonthDays(_ date: Date, calendar: Calendar) -> [Date] {
        let all=monthDays(date,calendar:calendar)
        let month=calendar.dateInterval(of:.month,for:date)!
        let last=calendar.date(byAdding:.day,value:-1,to:month.end)!
        let offset=calendar.dateComponents([.day],from:all[0],to:last).day!
        return Array(all.prefix(((offset/7)+1)*7))
    }
    struct Placement: Identifiable {
        let record: PagerRecord, lane: Int, lanes: Int
        var id: UInt32 {record.id}
        var start: Int {Int(record.integer(8)*60+record.integer(9))}
        var end: Int {Int(record.integer(10)*60+record.integer(11))}
    }
    // Partition overlapping clusters so simultaneous lessons never obscure one another.
    static func placements(_ events: [PagerRecord]) -> [Placement] {
        let sorted=events.sorted {
            let a=$0.integer(8)*60+$0.integer(9),b=$1.integer(8)*60+$1.integer(9)
            return a == b ? $0.id < $1.id : a < b
        }
        var result:[Placement]=[],cluster:[(PagerRecord,Int)]=[],ends:[Int]=[],clusterEnd=0
        func flush() {
            result += cluster.map {Placement(record:$0.0,lane:$0.1,lanes:ends.count)}
            cluster=[];ends=[];clusterEnd=0
        }
        for record in sorted {
            let start=Int(record.integer(8)*60+record.integer(9)),end=Int(record.integer(10)*60+record.integer(11))
            if !cluster.isEmpty && start >= clusterEnd {flush()}
            let lane=ends.firstIndex(where:{$0 <= start}) ?? ends.count
            if lane == ends.count {ends.append(end)} else {ends[lane]=end}
            cluster.append((record,lane));clusterEnd=max(clusterEnd,end)
        }
        flush();return result
    }
}
