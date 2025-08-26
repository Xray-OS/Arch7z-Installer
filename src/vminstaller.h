#pragma once
#include "installer.h"

class VMInstaller : public Installer {
    Q_OBJECT

public:
    VMInstaller(const InstallConfig &config, QObject *parent = nullptr);

private:
    void partitionDisk() override;
    void formatPartitions() override;
    void mountPartitions() override;
    void installBaseSystem() override;
    void installBootloader() override;
    
    QString getRootPartition() const;
};