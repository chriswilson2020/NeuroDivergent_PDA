import SwiftUI

private struct CalendarEdit: Identifiable {
    let record: PagerRecord
    var id: UInt32 {record.id}
}

struct DesktopCalendarView: View {
    @ObservedObject var model: PagerDesktopModel
    @State private var mode="Week"
    @State private var date=Date()
    @State private var search=""
    @State private var editing: CalendarEdit?
    @State private var deleting: PagerRecord?
    @State private var error: String?
    @State private var selectedEvent: UInt32?
    @State private var bulk=false
    @AppStorage("calendarDefaultView") private var defaultView="Week"
    private let schema=PagerSchema.named("calendar")
    private let hourHeight: CGFloat=76
    private let timeGutter: CGFloat=60
    private let dayHeaderHeight: CGFloat=72
    private var calendar: Calendar {model.calendar}
    private var days:[Date] {
        if mode == "Day" {return [calendar.startOfDay(for:date)]}
        let start=DesktopCalendarLayout.weekStart(date,calendar:calendar)
        return (0..<7).map {calendar.date(byAdding:.day,value:$0,to:start)!}
    }
    private func text(_ date: Date,_ format: String) -> String {
        let f=DateFormatter();f.calendar=calendar;f.timeZone=calendar.timeZone;f.dateFormat=format
        return f.string(from:date)
    }
    private var heading: String {
        if mode == "Agenda" {return "Calendar agenda"}
        if mode == "Day" {return text(date,"EEEE, d MMMM yyyy")}
        if mode == "Month" {return text(date,"MMMM yyyy")}
        return "\(text(days[0],"d MMM")) – \(text(days[6],"d MMM yyyy"))"
    }
    private func events(_ day: Date) -> [PagerRecord] {
        model.events(on:day).filter {search.isEmpty || $0.title(schema).localizedCaseInsensitiveContains(search) || $0.text(55,20).localizedCaseInsensitiveContains(search)}
    }
    var body: some View {
        VStack(spacing:0) {
            HStack(alignment:.center,spacing:16) {
                VStack(alignment:.leading,spacing:4) {
                    Text(heading).font(.title2.weight(.semibold)).lineLimit(1).minimumScaleFactor(0.85)
                    Text("\(model.items("calendar").count) saved events · \(calendar.timeZone.identifier)")
                        .font(.caption).foregroundStyle(.secondary)
                }.frame(maxWidth:.infinity,alignment:.leading)
                Picker("View",selection:$mode) {ForEach(["Day","Week","Month","Agenda"],id:\.self) {Text($0)}}
                    .pickerStyle(.segmented).labelsHidden().frame(width:260)
            }.padding(.horizontal,20).padding(.top,18).padding(.bottom,14)
            if mode != "Agenda" {
            DesktopFlow {
                HStack(spacing:8) {
                    Button {move(-1)} label:{Image(systemName:"chevron.left").frame(width:24)}.help("Previous \(mode.lowercased())")
                    Button("Today") {date=Date()}
                    Button {move(1)} label:{Image(systemName:"chevron.right").frame(width:24)}.help("Next \(mode.lowercased())")
                }
                DatePicker("Go to",selection:$date,displayedComponents:.date).labelsHidden().datePickerStyle(.stepperField).fixedSize()
                HStack(spacing:6) {
                    Image(systemName:"magnifyingglass").foregroundStyle(.secondary)
                    TextField("Search events",text:$search).textFieldStyle(.plain)
                    if !search.isEmpty {Button {search=""} label:{Image(systemName:"xmark.circle.fill")}.buttonStyle(.plain).foregroundStyle(.secondary)}
                }.padding(7).frame(minWidth:100,maxWidth:210).background(.quaternary,in:RoundedRectangle(cornerRadius:7))
                Button {create(on:date)} label:{Label("New Event",systemImage:"plus")}
                    .buttonStyle(.borderedProminent).keyboardShortcut("n").fixedSize()
                Button("Bulk Tools…") {bulk=true}.fixedSize()
                if let id=selectedEvent, let record=model.items("calendar").first(where:{$0.id == id}) {
                    Button("Edit Selected…") {editing=CalendarEdit(record:record)}.fixedSize()
                }
            }.padding(.horizontal,20).padding(.bottom,16)
            }
            Divider()
            if let issue=model.errors["calendar"] {
                ContentUnavailableView("Could not read Calendar",systemImage:"exclamationmark.triangle",description:Text(issue))
            } else if mode == "Agenda" {
                DesktopRecordsView(model:model,schema:schema)
            } else if mode == "Month" {monthView} else {timeView}
        }
        .disabled(!model.connected)
        .onAppear {mode=["Day","Week","Month","Agenda"].contains(defaultView) ? defaultView : "Week"}
        .sheet(item:$editing) {item in PagerRecordEditor(model:model,schema:schema,record:item.record).id(item.id)}
        .sheet(isPresented:$bulk) {CompanionCalendarToolsView(model:model)}
        .alert("Calendar",isPresented:Binding(get:{error != nil},set:{if !$0 {error=nil}})) {Button("OK") {}} message:{Text(error ?? "")}
        .confirmationDialog("Delete this event? For repeating events this deletes the entire series.",isPresented:Binding(get:{deleting != nil},set:{if !$0 {deleting=nil}}),titleVisibility:.visible) {
            Button("Delete event",role:.destructive) {
                if let record=deleting {do {try model.remove(record,schema:schema)} catch {self.error=error.localizedDescription}}
                deleting=nil
            }
            Button("Cancel",role:.cancel) {deleting=nil}
        }
    }
    private func move(_ amount: Int) {
        date=calendar.date(byAdding:mode == "Month" ? .month : .day,value:mode == "Week" ? amount*7 : amount,to:date)!
    }
    private func create(on day: Date,hour: Int=9) {
        do {
            var record=try model.create(schema)
            let d=calendar.dateComponents([.year,.month,.day],from:day)
            record.setInteger(4,2,UInt64(d.year!));record.setInteger(6,1,UInt64(d.month!));record.setInteger(7,1,UInt64(d.day!))
            record.setInteger(8,1,UInt64(hour));record.setInteger(9,1,0)
            record.setInteger(10,1,UInt64(min(23,hour+1)));record.setInteger(11,1,hour == 23 ? 59 : 0)
            editing=CalendarEdit(record:record)
        } catch {self.error=error.localizedDescription}
    }
    private func eventTime(_ record: PagerRecord) -> String {
        String(format:"%02d:%02d–%02d:%02d",Int(record.integer(8)),Int(record.integer(9)),Int(record.integer(10)),Int(record.integer(11)))
    }
    private func eventButton(_ record: PagerRecord,compact: Bool=false,height: CGFloat=80) -> some View {
        Button {selectedEvent=record.id;editing=CalendarEdit(record:record)} label:{
            VStack(alignment:.leading,spacing:2) {
                Text(compact ? String(format:"%02d:%02d %@",Int(record.integer(8)),Int(record.integer(9)),record.title(schema)) : record.title(schema))
                    .font(.system(size:compact ? 11 : 12,weight:.semibold)).lineLimit(compact || height < 60 ? 1 : 2)
                if !compact && height >= 44 {
                    Text(eventTime(record)).font(.system(size:11)).foregroundStyle(.secondary).lineLimit(1)
                    if height >= 72 && !record.text(55,20).isEmpty {Text(record.text(55,20)).font(.system(size:11)).foregroundStyle(.secondary).lineLimit(1)}
                }
            }.padding(.leading,compact ? 6 : 8).padding(.trailing,4).padding(.vertical,compact ? 3 : 5)
                .frame(maxWidth:.infinity,maxHeight:.infinity,alignment:.topLeading)
                .foregroundStyle(Color.primary).background(Color.accentColor.opacity(0.17))
                .overlay(alignment:.leading) {Rectangle().fill(Color.accentColor).frame(width:3).allowsHitTesting(false)}
                .clipShape(RoundedRectangle(cornerRadius:5)).contentShape(Rectangle())
        }.buttonStyle(.plain)
        .help("\(record.title(schema)) · \(eventTime(record)) · \(record.text(55,20))\(record.integer(14) != 0 ? " · Repeating series" : "")")
        .contextMenu {
            Button("Edit\(record.integer(14) != 0 ? " series" : " event")…") {editing=CalendarEdit(record:record)}
            Button("Delete…",role:.destructive) {deleting=record}
        }
    }
    private var timeView: some View {
        GeometryReader {geometry in
            let columnWidth=mode == "Week" ? max(128,(geometry.size.width-timeGutter)/7) : max(128,geometry.size.width-timeGutter)
            let contentWidth=timeGutter + columnWidth*CGFloat(days.count)
            ScrollView(.horizontal) {
            VStack(spacing:0) {
            HStack(spacing:0) {
                Text("TIME").font(.system(size:9,weight:.semibold)).foregroundStyle(.tertiary).frame(width:timeGutter)
                ForEach(days,id:\.self) {day in
                    Button {date=day;mode="Day"} label:{
                        VStack(spacing:4) {
                            Text(text(day,"EEE").uppercased()).font(.system(size:10,weight:.semibold))
                            Text(text(day,"d")).font(.system(size:20,weight:.semibold))
                                .frame(width:32,height:32).background(calendar.isDateInToday(day) ? Color.accentColor.opacity(0.14) : .clear,in:Circle())
                        }
                            .foregroundStyle(calendar.isDateInToday(day) ? Color.accentColor : .primary)
                            .frame(width:columnWidth).padding(.vertical,10)
                    }.buttonStyle(.plain)
                }
            }.frame(width:contentWidth,height:dayHeaderHeight).clipped()
            Divider()
            ScrollViewReader {proxy in
                ScrollView(.vertical) {
                    HStack(alignment:.top,spacing:0) {
                        VStack(spacing:0) {
                            ForEach(0..<24,id:\.self) {hour in
                                Text(String(format:"%02d:00",hour)).font(.system(size:10)).monospacedDigit().foregroundStyle(.secondary)
                                    .padding(.trailing,10).frame(width:timeGutter,height:hourHeight,alignment:.topTrailing).id(hour)
                            }
                        }
                        ForEach(days,id:\.self) {day in dayColumn(day).frame(width:columnWidth)}
                    }.padding(.top,8).frame(width:contentWidth)
                }.frame(height:max(1,geometry.size.height-dayHeaderHeight-1))
                    .scrollIndicators(.hidden).onAppear {proxy.scrollTo(8,anchor:.top)}
            }
            }.frame(width:contentWidth,height:geometry.size.height,alignment:.top)
            }
        }
    }
    private func dayColumn(_ day: Date) -> some View {
        GeometryReader {geometry in
            ZStack(alignment:.topLeading) {
                (calendar.isDateInToday(day) ? Color.accentColor.opacity(0.035) : Color.clear)
                VStack(spacing:0) {
                    ForEach(0..<24,id:\.self) {hour in
                        Rectangle().fill(Color.clear).frame(height:hourHeight)
                            .overlay(alignment:.top) {Rectangle().fill(Color.secondary.opacity(0.2)).frame(height:1)}
                            .overlay(alignment:.center) {Rectangle().fill(Color.secondary.opacity(0.08)).frame(height:1)}
                            .contentShape(Rectangle()).onTapGesture(count:2) {create(on:day,hour:hour)}
                    }
                }
                ForEach(DesktopCalendarLayout.placements(events(day))) {placement in
                    let width=max(1,(geometry.size.width-8)/CGFloat(placement.lanes))
                    let height=max(20,CGFloat(placement.end-placement.start)*hourHeight/60)-3
                    eventButton(placement.record,height:height)
                        .frame(width:max(1,width-3),height:height)
                        .offset(x:CGFloat(placement.lane)*width+4,y:CGFloat(placement.start)*hourHeight/60)
                }
            }.overlay(alignment:.trailing) {Rectangle().fill(Color.secondary.opacity(0.2)).frame(width:1).allowsHitTesting(false)}
        }.frame(height:hourHeight*24)
    }
    private var monthView: some View {
        let dates=DesktopCalendarLayout.visibleMonthDays(date,calendar:calendar)
        return VStack(spacing:0) {
            HStack(spacing:0) {
                ForEach(Array(dates.prefix(7)),id:\.self) {day in Text(text(day,"EEE").uppercased()).font(.system(size:10,weight:.semibold)).foregroundStyle(.secondary).frame(maxWidth:.infinity).padding(10)}
            }.frame(height:34)
            GeometryReader {geometry in
                let rowHeight=max(112,(geometry.size.height-CGFloat(dates.count/7-1))/CGFloat(dates.count/7))
                ScrollView(.vertical) {
                LazyVGrid(columns:Array(repeating:GridItem(.flexible(minimum:0),spacing:1),count:7),spacing:1) {
                    ForEach(dates,id:\.self) {day in
                        monthCell(day,height:rowHeight)
                    }
                }.background(Color.secondary.opacity(0.2))
                }.scrollIndicators(.hidden)
            }
        }
    }
    private func monthCell(_ day: Date,height: CGFloat) -> some View {
        let items=events(day),limit=max(1,Int((height-58)/25))
        return VStack(alignment:.leading,spacing:3) {
            HStack {
                Button {date=day;mode="Day"} label:{Text(text(day,calendar.component(.day,from:day) == 1 ? "d MMM" : "d")).font(.system(size:12,weight:.semibold))
                    .padding(.horizontal,6).frame(height:26)
                    .background(calendar.isDateInToday(day) ? Color.accentColor.opacity(0.15) : .clear,in:Capsule())
                    .foregroundStyle(calendar.isDateInToday(day) ? Color.accentColor : (calendar.component(.month,from:day) == calendar.component(.month,from:date) ? .primary : .secondary))}.buttonStyle(.plain)
                Spacer()
                Button {create(on:day)} label:{Image(systemName:"plus").font(.caption2)}.buttonStyle(.plain).help("New event on \(text(day,"d MMM"))")
            }
            ForEach(Array(items.prefix(limit))) {record in eventButton(record,compact:true).frame(height:22)}
            if items.count > limit {Button("+\(items.count-limit) more") {date=day;mode="Day"}.font(.caption).buttonStyle(.plain)}
            Spacer(minLength:0)
        }.padding(7).frame(maxWidth:.infinity).frame(height:height).clipped()
            .background(calendar.component(.month,from:day) == calendar.component(.month,from:date) ? Color(nsColor:.controlBackgroundColor) : Color(nsColor:.windowBackgroundColor))
    }
}
