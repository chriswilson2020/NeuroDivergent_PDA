#pragma once
#include <vector>
#include <stdint.h>
struct TaskRecord {
    uint32_t id=1;uint8_t completed=0,reminder=1;
    int16_t dueYear=2026;uint8_t dueMonth=10,dueDay=10;
    char title[48]="Task",notes[96]="";
};
class TaskStore {
public:
    std::vector<TaskRecord> tasks;
    size_t count()const{return tasks.size();}
    const TaskRecord &at(size_t i)const{return tasks[i];}
};
