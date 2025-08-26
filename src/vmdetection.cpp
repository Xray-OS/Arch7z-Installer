#include "vmdetection.h"
#include <QFile>
#include <QTextStream>
#include <QProcess>
#include <QDir>

bool VMDetection::isVirtualMachine() {
    QProcess detectVirt;
    detectVirt.start("systemd-detect-virt", QStringList() << "-v");
    if (detectVirt.waitForFinished(2000)) {
        return detectVirt.exitCode() == 0;
    }
    return false;
}

QString VMDetection::getVirtualizationType() {
    QProcess detectVirt;
    detectVirt.start("systemd-detect-virt");
    if (detectVirt.waitForFinished(2000) && detectVirt.exitCode() == 0) {
        QString result = detectVirt.readAllStandardOutput().trimmed();
        if (!result.isEmpty() && result != "none") {
            return result;
        }
    }
    return "none";
}

bool VMDetection::checkDMI() {
    QStringList dmiFiles = {
        "/sys/class/dmi/id/sys_vendor",
        "/sys/class/dmi/id/product_name",
        "/sys/class/dmi/id/board_vendor"
    };
    
    QStringList vmIndicators = {
        "vmware", "virtualbox", "qemu", "kvm", "xen", 
        "microsoft corporation", "parallels", "bochs"
    };
    
    for (const QString &file : dmiFiles) {
        if (!QFile::exists(file)) continue;
        
        QFile dmiFile(file);
        if (dmiFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream stream(&dmiFile);
            QString content = stream.readAll().trimmed().toLower();
            dmiFile.close();
            
            for (const QString &indicator : vmIndicators) {
                if (content.contains(indicator)) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool VMDetection::checkCPUFlags() {
    if (!QFile::exists("/proc/cpuinfo")) return false;
    
    QFile cpuinfo("/proc/cpuinfo");
    if (cpuinfo.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&cpuinfo);
        QString line;
        while (stream.readLineInto(&line)) {
            if (line.startsWith("flags") && line.contains("hypervisor")) {
                cpuinfo.close();
                return true;
            }
        }
        cpuinfo.close();
    }
    return false;
}

bool VMDetection::checkDevices() {
    // Check for VM-specific devices
    QStringList vmDevices = {
        "/dev/vda", "/dev/vdb",  // VirtIO
        "/sys/bus/pci/devices/0000:00:01.1/vendor"  // VMware
    };
    
    for (const QString &device : vmDevices) {
        if (QFile::exists(device)) {
            return true;
        }
    }
    
    // Check PCI devices for VM indicators
    QDir pciDir("/sys/bus/pci/devices");
    if (pciDir.exists()) {
        for (const QString &entry : pciDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            QFile vendorFile(pciDir.absoluteFilePath(entry) + "/vendor");
            if (vendorFile.open(QIODevice::ReadOnly)) {
                QString vendor = vendorFile.readAll().trimmed();
                // VMware: 0x15ad, VirtualBox: 0x80ee, QEMU: 0x1af4
                if (vendor == "0x15ad" || vendor == "0x80ee" || vendor == "0x1af4") {
                    return true;
                }
            }
        }
    }
    
    return false;
}