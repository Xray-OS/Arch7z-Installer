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
    
    QString partitionScript = QString(R"(
#!/bin/bash
set -e

# Validate disk exists
test -b %1 || (echo 'Selected disk does not exist: %1' && exit 1)

# Unmount any existing partitions
umount %1* 2>/dev/null || true

# Wipe existing partition table
dd if=/dev/zero of=%1 bs=1M count=1 2>/dev/null || true
sync
sleep 1

# Create MBR partition table with single bootable Linux partition
echo ',,L,*' | sfdisk %1
sync
sleep 2

# Force kernel to re-read partition table
partprobe %1
sync
sleep 3

# Wait for udev to create device nodes
udevadm settle
sleep 2

# Validate partition was created
ROOT_PART="%2"
echo "Checking for root partition: $ROOT_PART"
test -b "$ROOT_PART" || (echo "Failed to create VM root partition: $ROOT_PART" && exit 1)

# Show final partition status
echo "Final VM partition layout:"
lsblk %1
)")
    .arg(config.selectedDisk)
    .arg(getRootPartition());
    
    executeCommand("bash", QStringList() << "-c" << partitionScript);
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
    qDebug() << "[DEBUG] VM installBootloader() - BIOS/Legacy GRUB configuration";
    
    QStringList bootloaderCommands;
    bootloaderCommands << "test -f /mnt/etc/fstab || (echo 'System not properly installed' && exit 1)";
    
    // Configure GRUB for BIOS boot
    bootloaderCommands << "arch-chroot /mnt sed -i 's/GRUB_CMDLINE_LINUX_DEFAULT=.*/GRUB_CMDLINE_LINUX_DEFAULT=\"quiet\"/' /etc/default/grub";
    bootloaderCommands << "arch-chroot /mnt sed -i 's/#GRUB_DISABLE_OS_PROBER=false/GRUB_DISABLE_OS_PROBER=false/' /etc/default/grub";
    bootloaderCommands << "arch-chroot /mnt sed -i 's/GRUB_TIMEOUT=.*/GRUB_TIMEOUT=5/' /etc/default/grub";
    
    // Note: GRUB installation and config generation moved to final-settings.conf
    
    QString bootloaderScript = bootloaderCommands.join(" && ");
    executeCommand("bash", QStringList() << "-c" << bootloaderScript);
}

void VMInstaller::installBaseSystem() {
    qDebug() << "[DEBUG] VM installBaseSystem() - Starting base system installation";
    
    QString installScript = R"(
#!/bin/bash
set -e

# Validate mount point
test -d /mnt || (echo '/mnt directory does not exist' && exit 1)
mountpoint -q /mnt || (echo '/mnt is not mounted' && exit 1)

# Try copytoram first, then bootmnt as fallback
if [ -f /run/archiso/copytoram/airootfs.sfs ]; then
    SQUASHFS_PATH='/run/archiso/copytoram/airootfs.sfs'
    echo 'VM: Using copytoram SquashFS'
elif [ -f /run/archiso/bootmnt/arch/x86_64/airootfs.sfs ]; then
    SQUASHFS_PATH='/run/archiso/bootmnt/arch/x86_64/airootfs.sfs'
    echo 'VM: Using bootmnt SquashFS'
else
    echo 'ERROR: No SquashFS found'
    exit 1
fi

# Install system from SquashFS
echo "Found SquashFS at: $SQUASHFS_PATH"
mkdir -p /tmp/squashfs-root
mount -t squashfs -o loop "$SQUASHFS_PATH" /tmp/squashfs-root
rsync -aHAXS --numeric-ids --exclude=/dev/* --exclude=/proc/* --exclude=/sys/* --exclude=/tmp/* --exclude=/run/* --exclude=/mnt/* --exclude=/media/* --exclude=/lost+found /tmp/squashfs-root/ /mnt/

# Copy kernel - prioritize SquashFS source over live environment
mkdir -p /mnt/boot
if [ -f /mnt/usr/lib/modules/$(uname -r)/vmlinuz ]; then
    cp /mnt/usr/lib/modules/$(uname -r)/vmlinuz /mnt/boot/vmlinuz-linux
elif [ -f /usr/lib/modules/$(uname -r)/vmlinuz ]; then
    cp /usr/lib/modules/$(uname -r)/vmlinuz /mnt/boot/vmlinuz-linux
elif [ -f /boot/vmlinuz-linux ]; then
    cp /boot/vmlinuz-linux /mnt/boot/vmlinuz-linux
else
    echo 'Kernel not found, skipping'
fi

# Cleanup
umount /tmp/squashfs-root
rmdir /tmp/squashfs-root

# Verify installation
test -d /mnt/etc || (echo 'System installation failed - /mnt/etc missing' && exit 1)
test -d /mnt/usr || (echo 'System installation failed - /mnt/usr missing' && exit 1)
)";
    
    executeCommand("bash", QStringList() << "-c" << installScript);
}

QString VMInstaller::getRootPartition() const {
    return config.selectedDisk + "1";
}