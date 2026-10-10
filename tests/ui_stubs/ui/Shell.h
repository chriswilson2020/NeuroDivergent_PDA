#pragma once
class Shell {
public:
    int backs=0,homes=0;
    void goToday(){}void toggleLauncher(){}void goCapture(){}void goTransition(){}
    void back(){++backs;}
    void goLauncher(){++homes;}
};
