#pragma once
class Shell {
public:
    int backs=0,homes=0;
    bool locked(){return false;}
    void userActivity(){}
    void goToday(){}void toggleLauncher(){}void goCapture(){}void goTransition(){}
    void back(){++backs;}
    void goLauncher(){++homes;}
};
