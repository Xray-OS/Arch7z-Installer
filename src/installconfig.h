#pragma once
#include <QString>

enum class PartitioningMode {
    Automatic,
    Manual
};

enum class InstallationSource {
    AutomaticInstall,
    CustomPartitioning
};

struct InstallConfig {
    // Locale settings
    QString language;
    QString region;
    QString timezone;
    QString keyboardLayout;
    QString keyboardVariant;
    
    // Disk settings
    QString selectedDisk;
    QString diskSize;
    bool enableSwap;
    QString filesystem;
    QString efiSize;
    QString swapSize;
    PartitioningMode partitioningMode;
    
    // Advanced partition settings
    QString bootPartition;
    QString rootPartition;
    QString swapPartition;
    QString bootFormat;
    
    // User settings
    QString hostname;
    QString username;
    QString password;
    QString rootPassword;
    bool samePassword;
    QString shell;
    
    // Installation source tracking
    InstallationSource installationSource;
    
    // VM detection
    bool isVirtualMachine;
    QString virtualizationType;
    
    InstallConfig() : enableSwap(true), samePassword(true), filesystem("btrfs"), partitioningMode(PartitioningMode::Automatic), installationSource(InstallationSource::AutomaticInstall), isVirtualMachine(false) {}
    
    void reset() {
        installationSource = InstallationSource::AutomaticInstall;
        partitioningMode = PartitioningMode::Automatic;
    }
};