#include "vminstaller.h"
#include <QDebug>

VMInstaller::VMInstaller(const InstallConfig &config, QObject *parent)
    : Installer(config, parent) {
}

void VMInstaller::partitionDisk() {
    qDebug() << "[DEBUG] VM partitionDisk() - Using MBR partitioning";
    
    if (config.partitioningMode == PartitioningMode::Manual) {
        QStringList validateCommands;
        validateCommands << QString("test -b %1 || (echo 'Root partition does not exist: %1' && exit 1)").arg(config.rootPartition);
        validateCommands << "lsblk";
        executeCommand("bash", QStringList() << "-c" << validateCommands.join(" && "));
        return;
    }
    
    qDebug() << "[DEBUG] VM automatic partitioning - MBR + single root partition";
    
    if (config.selectedDisk.isEmpty()) {
        emit installationFinished(false, "VM partitioning requires a disk to be selected");
        return;
    }
    
    QStringList commands;
    commands << QString("dd if=/dev/zero of=%1 bs=1M count=1").arg(config.selectedDisk);
    commands << QString("echo ',,L,*' | sfdisk %1").arg(config.selectedDisk);
    commands << "sync && sleep 2";
    commands << QString("test -b %1").arg(getRootPartition());
    
    executeCommand("bash", QStringList() << "-c" << commands.join(" && "));
}

void VMInstaller::formatPartitions() {
    qDebug() << "[DEBUG] VM formatPartitions() - Single partition formatting";
    
    QString rootPartition;
    if (config.partitioningMode == PartitioningMode::Manual) {
        rootPartition = config.rootPartition;
    } else {
        rootPartition = getRootPartition();
    }
    
    QStringList formatCommands;
    formatCommands << "sleep 2";
    formatCommands << QString("test -b %1 || (echo 'Root partition not found: %1' && exit 1)").arg(rootPartition);
    
    // Format single root partition
    if (config.filesystem == "btrfs") {
        formatCommands << QString("mkfs.btrfs -f %1").arg(rootPartition);
    } else {
        formatCommands << QString("mkfs.ext4 -F %1").arg(rootPartition);
    }
    
    QString formatScript = formatCommands.join(" && ");
    executeCommand("bash", QStringList() << "-c" << formatScript);
}

void VMInstaller::mountPartitions() {
    qDebug() << "[DEBUG] VM mountPartitions() - Single partition mount";
    
    QString rootPartition;
    if (config.partitioningMode == PartitioningMode::Manual) {
        rootPartition = config.rootPartition;
    } else {
        rootPartition = getRootPartition();
    }
    
    QStringList mountCommands;
    mountCommands << QString("test -b %1 || (echo 'Root partition not found: %1' && exit 1)").arg(rootPartition);
    mountCommands << "mkdir -p /mnt";
    
    if (config.filesystem == "btrfs") {
        // Btrfs with subvolumes
        mountCommands << QString("mount %1 /mnt").arg(rootPartition);
        mountCommands << "btrfs subvolume create /mnt/@";
        mountCommands << "btrfs subvolume create /mnt/@home";
        mountCommands << "umount /mnt";
        mountCommands << QString("mount -o subvol=@,compress=zstd %1 /mnt").arg(rootPartition);
        mountCommands << "mkdir -p /mnt/home";
        mountCommands << QString("mount -o subvol=@home,compress=zstd %1 /mnt/home").arg(rootPartition);
    } else {
        // Ext4 simple mount
        mountCommands << QString("mount %1 /mnt").arg(rootPartition);
        mountCommands << "mkdir -p /mnt/home";
    }
    
    // Create boot directory (no separate EFI partition in VM)
    mountCommands << "mkdir -p /mnt/boot";
    
    // Validate mount
    mountCommands << "mountpoint -q /mnt || (echo 'Failed to mount root partition' && exit 1)";
    mountCommands << "echo 'VM mount status:'";
    mountCommands << "findmnt /mnt";
    
    QString mountScript = mountCommands.join(" && ");
    executeCommand("bash", QStringList() << "-c" << mountScript);
}

void VMInstaller::installBootloader() {
    qDebug() << "[DEBUG] VM installBootloader() - BIOS/Legacy GRUB";
    
    QStringList bootloaderCommands;
    bootloaderCommands << "test -f /mnt/etc/fstab || (echo 'System not properly installed' && exit 1)";
    
    // Configure GRUB for BIOS boot
    bootloaderCommands << "arch-chroot /mnt sed -i 's/GRUB_CMDLINE_LINUX_DEFAULT=.*/GRUB_CMDLINE_LINUX_DEFAULT=\"quiet\"/' /etc/default/grub";
    bootloaderCommands << "arch-chroot /mnt sed -i 's/#GRUB_DISABLE_OS_PROBER=false/GRUB_DISABLE_OS_PROBER=false/' /etc/default/grub";
    bootloaderCommands << "arch-chroot /mnt sed -i 's/GRUB_TIMEOUT=.*/GRUB_TIMEOUT=5/' /etc/default/grub";
    
    // Install GRUB to MBR (BIOS mode)
    bootloaderCommands << QString("arch-chroot /mnt grub-install --target=i386-pc %1").arg(config.selectedDisk);
    
    // Generate GRUB configuration
    bootloaderCommands << "arch-chroot /mnt grub-mkconfig -o /boot/grub/grub.cfg";
    
    QString bootloaderScript = bootloaderCommands.join(" && ");
    executeCommand("bash", QStringList() << "-c" << bootloaderScript);
}

void VMInstaller::installBaseSystem() {
    qDebug() << "[DEBUG] VM installBaseSystem() - Starting base system installation";
    
    QString squashfsPath = "/run/archiso/bootmnt/arch/x86_64/airootfs.sfs";
    qDebug() << "[DEBUG] VM installBaseSystem() - Looking for SquashFS at" << squashfsPath;
    
    QStringList installCommands;
    installCommands << "test -d /mnt";
    installCommands << "mountpoint -q /mnt";
    installCommands << QString("test -f %1").arg(squashfsPath);
    installCommands << "mkdir -p /tmp/squashfs-root";
    installCommands << QString("mount -t squashfs -o loop %1 /tmp/squashfs-root").arg(squashfsPath);
    installCommands << "rsync -aHAXS --numeric-ids --exclude=/dev/* --exclude=/proc/* --exclude=/sys/* --exclude=/tmp/* --exclude=/run/* --exclude=/mnt/* --exclude=/media/* --exclude=/lost+found /tmp/squashfs-root/ /mnt/";
    installCommands << "mkdir -p /mnt/boot";
    installCommands << "test -f /run/archiso/bootmnt/arch/boot/x86_64/vmlinuz-linux && cp /run/archiso/bootmnt/arch/boot/x86_64/vmlinuz-linux /mnt/boot/vmlinuz-linux || echo 'Kernel not found, skipping'";
    installCommands << "umount /tmp/squashfs-root";
    installCommands << "rmdir /tmp/squashfs-root";
    installCommands << "test -d /mnt/etc";
    installCommands << "test -d /mnt/usr";
    
    executeCommand("bash", QStringList() << "-c" << installCommands.join(" && "));
}

QString VMInstaller::getRootPartition() const {
    return config.selectedDisk + "1";
}