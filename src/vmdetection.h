#pragma once
#include <QString>

class VMDetection {
public:
    static bool isVirtualMachine();
    static QString getVirtualizationType();
    
private:
    static bool checkDMI();
    static bool checkCPUFlags();
    static bool checkDevices();
};