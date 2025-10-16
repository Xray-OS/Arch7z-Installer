#include "installer.h"
#include "vmdetection.h"
#include <QDebug>
#include <QDir>
#include <QTextStream>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QRegularExpression>
#include <QCoreApplication>
#include <unistd.h>

Installer::Installer(const InstallConfig &config, QObject *parent)
    : QObject(parent), config(config), currentProcess(nullptr), currentStep(0), totalSteps(8) {
    
    installSteps << "Partitioning disk"
                << "Formatting partitions" 
                << "Mounting partitions"
                << "Installing base system"
                << "Configuring system"
                << "Installing bootloader"
                << "Running custom scripts"
                << "Cleanup";
                
    progressTimer = new QTimer(this);
    progressTimer->setSingleShot(true);
    connect(progressTimer, &QTimer::timeout, this, &Installer::executeNextStep);
}

void Installer::startInstallation() {
    currentStep = 0;
    updateProgress(0, "Starting installation...");
    progressTimer->start(1000);
}

void Installer::executeNextStep() {
    QString stepMessage = installSteps[currentStep];
    int percentage = (currentStep * 100) / totalSteps;
    qDebug() << QString("[DEBUG] Step %1/%2: %3 (%4%)").arg(currentStep+1).arg(totalSteps).arg(stepMessage).arg(percentage);
    updateProgress(percentage, stepMessage);
    
    switch (currentStep) {
        case 0: qDebug() << "[DEBUG] Starting partitionDisk()"; partitionDisk(); break;
        case 1: qDebug() << "[DEBUG] Starting formatPartitions()"; formatPartitions(); break;
        case 2: qDebug() << "[DEBUG] Starting mountPartitions()"; mountPartitions(); break;
        case 3: qDebug() << "[DEBUG] Starting installBaseSystem()"; installBaseSystem(); break;
        case 4: qDebug() << "[DEBUG] Starting configureSystem()"; configureSystem(); break;
        case 5: qDebug() << "[DEBUG] Starting installBootloader()"; installBootloader(); break;
        case 6: qDebug() << "[DEBUG] Starting runCustomScripts()"; runCustomScripts(); break;
        case 7: qDebug() << "[DEBUG] Starting cleanup()"; cleanup(); break;
        default:
            qDebug() << "[ERROR] Invalid step:" << currentStep;
            emit installationFinished(false, "Invalid installation step");
            break;
    }
}

void Installer::partitionDisk() {
    qDebug() << "[DEBUG] partitionDisk() - Starting disk partitioning";
    qDebug() << "[DEBUG] Partitioning mode:" << (config.partitioningMode == PartitioningMode::Manual ? "Manual" : "Automatic");
    
    if (config.partitioningMode == PartitioningMode::Manual) {
        qDebug() << "[DEBUG] Manual partitioning mode";
        qDebug() << "[DEBUG] Boot partition:" << config.bootPartition;
        qDebug() << "[DEBUG] Root partition:" << config.rootPartition;
        qDebug() << "[DEBUG] Swap partition:" << config.swapPartition;
        
        // Validate manual partitioning configuration
        if (config.bootPartition.isEmpty() || config.rootPartition.isEmpty()) {
            QString errorMsg = "Manual partitioning mode requires both boot and root partitions to be assigned";
            qDebug() << "[FATAL]" << errorMsg;
            emit installationFinished(false, errorMsg);
            return;
        }
        
        // In manual mode, partitions are already created - just validate they exist
        QStringList validateCommands;
        validateCommands << QString("test -b %1 || (echo 'Boot partition does not exist: %1' && exit 1)").arg(config.bootPartition);
        validateCommands << QString("test -b %1 || (echo 'Root partition does not exist: %1' && exit 1)").arg(config.rootPartition);
        
        if (!config.swapPartition.isEmpty()) {
            validateCommands << QString("test -b %1 || (echo 'Swap partition does not exist: %1' && exit 1)").arg(config.swapPartition);
        }
        
        // Show current partition status
        validateCommands << "lsblk";
        
        QString validateScript = validateCommands.join(" && ");
        executeCommand("bash", QStringList() << "-c" << validateScript);
        return;
    }
    
    // Automatic partitioning mode
    qDebug() << "[DEBUG] Automatic partitioning mode";
    qDebug() << "[DEBUG] Selected disk:" << config.selectedDisk;
    qDebug() << "[DEBUG] Filesystem:" << config.filesystem;
    qDebug() << "[DEBUG] Enable swap:" << config.enableSwap;
    
    // Validate automatic partitioning configuration
    if (config.selectedDisk.isEmpty()) {
        QString errorMsg = "Automatic partitioning mode requires a disk to be selected";
        qDebug() << "[FATAL]" << errorMsg;
        emit installationFinished(false, errorMsg);
        return;
    }
    
    QString fsType = (config.filesystem == "btrfs") ? "btrfs" : "ext4";
    
    // Build complete partitioning script with validation
    QStringList partitionCommands;
    
    // Validate disk exists
    partitionCommands << QString("test -b %1 || (echo 'Selected disk does not exist: %1' && exit 1)").arg(config.selectedDisk);
    
    // Unmount any existing partitions (safety measure)
    partitionCommands << QString("umount %1* 2>/dev/null || true").arg(config.selectedDisk);
    
    // Build parted command
    QStringList partedCommands;
    partedCommands << "mklabel" << "gpt";
    partedCommands << "mkpart" << "primary" << "fat32" << "1MiB" << "2GiB";
    partedCommands << "set" << "1" << "esp" << "on";
    
    if (config.enableSwap) {
        partedCommands << "mkpart" << "primary" << "linux-swap" << "2GiB" << "11010MiB";
        partedCommands << "mkpart" << "primary" << fsType << "11010MiB" << "100%";
    } else {
        partedCommands << "mkpart" << "primary" << fsType << "2GiB" << "100%";
    }
    
    // Execute partitioning
    QString partedScript = partedCommands.join(" ");
    partitionCommands << QString("parted %1 --script %2").arg(config.selectedDisk).arg(partedScript);
    
    // Wait for kernel to recognize new partitions
    partitionCommands << "partprobe";
    partitionCommands << "sleep 2";
    
    // Validate partitions were created
    QString efiPartition = getPartitionName(config.selectedDisk, 1);
    QString rootPartition = getPartitionName(config.selectedDisk, config.enableSwap ? 3 : 2);
    partitionCommands << QString("test -b %1 || (echo 'Failed to create EFI partition: %1' && exit 1)").arg(efiPartition);
    partitionCommands << QString("test -b %1 || (echo 'Failed to create root partition: %1' && exit 1)").arg(rootPartition);
    
    if (config.enableSwap) {
        QString swapPartition = getPartitionName(config.selectedDisk, 2);
        partitionCommands << QString("test -b %1 || (echo 'Failed to create swap partition: %1' && exit 1)").arg(swapPartition);
    }
    
    // Show partition table for debugging
    partitionCommands << QString("lsblk %1").arg(config.selectedDisk);
    
    QString fullScript = partitionCommands.join(" && ");
    executeCommand("bash", QStringList() << "-c" << fullScript);
}

QString Installer::getPartitionName(const QString &disk, int partitionNumber) {
    // NVMe drives: /dev/nvme0n1 -> /dev/nvme0n1p1
    // MMC/eMMC: /dev/mmcblk0 -> /dev/mmcblk0p1  
    // Loop devices: /dev/loop0 -> /dev/loop0p1
    // Regular drives: /dev/sda -> /dev/sda1
    QRegularExpression needsP("(nvme\\d+n\\d+|mmcblk\\d+|loop\\d+)$");
    return needsP.match(disk).hasMatch() ? disk + "p" + QString::number(partitionNumber) : disk + QString::number(partitionNumber);
}

void Installer::formatPartitions() {
    qDebug() << "[DEBUG] formatPartitions() - Starting partition formatting";
    
    QString efiPartition, rootPartition, swapPartition;
    
    if (config.partitioningMode == PartitioningMode::Manual) {
        efiPartition = config.bootPartition;
        rootPartition = config.rootPartition;
        swapPartition = config.swapPartition;
        qDebug() << "[DEBUG] Manual partitioning - using assigned partitions";
    } else {
        efiPartition = getPartitionName(config.selectedDisk, 1);
        rootPartition = getPartitionName(config.selectedDisk, config.enableSwap ? 3 : 2);
        if (config.enableSwap) {
            swapPartition = getPartitionName(config.selectedDisk, 2);
        }
        qDebug() << "[DEBUG] Automatic partitioning - using generated partition names";
    }
    
    qDebug() << "[DEBUG] EFI partition:" << efiPartition;
    qDebug() << "[DEBUG] Root partition:" << rootPartition;
    
    // Build complete formatting script that includes waiting and validation
    QStringList formatCommands;
    
    // Wait for partitions to be available
    formatCommands << "sleep 2";
    
    // Verify partitions exist before formatting
    formatCommands << QString("test -b %1 || (echo 'EFI partition not found: %1' && exit 1)").arg(efiPartition);
    formatCommands << QString("test -b %1 || (echo 'Root partition not found: %1' && exit 1)").arg(rootPartition);
    
    // Format EFI partition
    formatCommands << QString("mkfs.fat -F32 %1").arg(efiPartition);
    
    // Handle swap partition if enabled
    if ((config.partitioningMode == PartitioningMode::Manual && !swapPartition.isEmpty()) || 
        (config.partitioningMode == PartitioningMode::Automatic && config.enableSwap)) {
        formatCommands << QString("test -b %1 || (echo 'Swap partition not found: %1' && exit 1)").arg(swapPartition);
        formatCommands << QString("mkswap %1").arg(swapPartition);
        formatCommands << QString("swapon %1").arg(swapPartition);
    }
    
    // Format root partition
    if (config.filesystem == "btrfs") {
        formatCommands << QString("mkfs.btrfs -f %1").arg(rootPartition);
    } else {
        formatCommands << QString("mkfs.ext4 -F %1").arg(rootPartition);
    }
    
    // Execute all formatting in one script
    QString formatScript = formatCommands.join(" && ");
    executeCommand("bash", QStringList() << "-c" << formatScript);
}

void Installer::mountPartitions() {
    qDebug() << "[DEBUG] mountPartitions() - Starting partition mounting";
    
    QString efiPartition, rootPartition;
    
    if (config.partitioningMode == PartitioningMode::Manual) {
        efiPartition = config.bootPartition;
        rootPartition = config.rootPartition;
        qDebug() << "[DEBUG] Manual partitioning - using assigned partitions";
    } else {
        efiPartition = getPartitionName(config.selectedDisk, 1);
        rootPartition = getPartitionName(config.selectedDisk, config.enableSwap ? 3 : 2);
        qDebug() << "[DEBUG] Automatic partitioning - using generated partition names";
    }
    
    qDebug() << "[DEBUG] Mounting root partition:" << rootPartition << "to /mnt";
    qDebug() << "[DEBUG] Mounting EFI partition:" << efiPartition << "to /mnt/boot/efi";
    
    QStringList mountCommands;
    
    // Validate partitions exist and are formatted
    mountCommands << QString("test -b %1 || (echo 'EFI partition not found: %1' && exit 1)").arg(efiPartition);
    mountCommands << QString("test -b %1 || (echo 'Root partition not found: %1' && exit 1)").arg(rootPartition);
    
    // Create mount point
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
    
    // Mount EFI partition
    mountCommands << "mkdir -p /mnt/boot/efi";
    mountCommands << QString("mount %1 /mnt/boot/efi").arg(efiPartition);
    
    // Validate mounts were successful
    mountCommands << "mountpoint -q /mnt || (echo 'Failed to mount root partition' && exit 1)";
    mountCommands << "mountpoint -q /mnt/boot/efi || (echo 'Failed to mount EFI partition' && exit 1)";
    
    // Show mount status for debugging
    mountCommands << "echo 'Mount status:'";
    mountCommands << "findmnt /mnt";
    mountCommands << "findmnt /mnt/boot/efi";
    
    // Execute all mounting in one script
    QString mountScript = mountCommands.join(" && ");
    executeCommand("bash", QStringList() << "-c" << mountScript);
}

void Installer::installBaseSystem() {
    qDebug() << "[DEBUG] installBaseSystem() - Starting base system installation";
    
    // Create a proper shell script instead of joining with &&
    QString installScript = R"(
#!/bin/bash
set -e

# No need to sync repos in live environment - we install from SquashFS

# Validate that /mnt exists and is mounted
test -d /mnt || (echo '/mnt directory does not exist' && exit 1)
mountpoint -q /mnt || (echo '/mnt is not mounted' && exit 1)
test -d /mnt/boot/efi || (echo '/mnt/boot/efi is not mounted' && exit 1)
mountpoint -q /mnt/boot/efi || (echo '/mnt/boot/efi is not mounted' && exit 1)

# Try multiple possible SquashFS locations with bootmnt as primary fallback
SQUASHFS_PATH=''
if [ -f /run/archiso/copytoram/airootfs.sfs ]; then
    SQUASHFS_PATH='/run/archiso/copytoram/airootfs.sfs'
    echo 'Using copytoram SquashFS'
elif [ -f /run/archiso/bootmnt/arch/x86_64/airootfs.sfs ]; then
    SQUASHFS_PATH='/run/archiso/bootmnt/arch/x86_64/airootfs.sfs'
    echo 'Using bootmnt SquashFS'
elif [ -f /run/archiso/sfs/airootfs/airootfs.sfs ]; then
    SQUASHFS_PATH='/run/archiso/sfs/airootfs/airootfs.sfs'
    echo 'Using sfs SquashFS'
else
    SQUASHFS_PATH=$(find /run -name 'airootfs.sfs' -type f 2>/dev/null | head -1)
    [ -n "$SQUASHFS_PATH" ] && echo "Found SquashFS at: $SQUASHFS_PATH"
fi

# Check available memory before extraction
MEM_AVAILABLE=$(awk '/MemAvailable/ {print $2}' /proc/meminfo)
echo "Available memory: ${MEM_AVAILABLE}KB"
if [ "$MEM_AVAILABLE" -lt 1048576 ]; then
    echo "WARNING: Low memory (${MEM_AVAILABLE}KB), forcing cleanup"
    echo 3 > /proc/sys/vm/drop_caches
    sync
fi

# Install base system from SquashFS
if [ -z "$SQUASHFS_PATH" ] || [ ! -f "$SQUASHFS_PATH" ]; then
    echo 'ERROR: No SquashFS found - cannot install without internet dependency'
    exit 1
else
    echo "Found SquashFS at: $SQUASHFS_PATH"
    
    # Extract directly without mounting to save memory
    if command -v unsquashfs >/dev/null 2>&1; then
        echo 'Using unsquashfs for direct extraction'
        cd /mnt
        # Extract with memory limits and progress
        unsquashfs -f -d . -p 1 "$SQUASHFS_PATH"
        
        # Clean cache every few seconds during extraction
        echo 1 > /proc/sys/vm/drop_caches &
    else
        echo 'Fallback: Using mount + cp (memory intensive)'
        mkdir -p /tmp/squashfs-root
        mount -t squashfs -o loop,ro "$SQUASHFS_PATH" /tmp/squashfs-root
        
        # Use cp instead of rsync for lower memory usage
        cp -a /tmp/squashfs-root/* /mnt/ 2>/dev/null || true
        
        # Immediate cleanup
        umount /tmp/squashfs-root
        rmdir /tmp/squashfs-root
    fi
    
    # Aggressive memory cleanup after extraction
    sync
    echo 3 > /proc/sys/vm/drop_caches
    
    # Check memory after extraction
    MEM_AFTER=$(awk '/MemAvailable/ {print $2}' /proc/meminfo)
    echo "Memory after extraction: ${MEM_AFTER}KB"
fi

# Copy kernel - prioritize SquashFS source over live environment
mkdir -p /mnt/boot
if [ -f /mnt/usr/lib/modules/$(uname -r)/vmlinuz ]; then
    cp /mnt/usr/lib/modules/$(uname -r)/vmlinuz /mnt/boot/vmlinuz-linux
elif [ -f /usr/lib/modules/$(uname -r)/vmlinuz ]; then
    cp /usr/lib/modules/$(uname -r)/vmlinuz /mnt/boot/vmlinuz-linux
elif [ -f /boot/vmlinuz-linux ]; then
    cp /boot/vmlinuz-linux /mnt/boot/vmlinuz-linux
else
    echo 'ERROR: Kernel not found in SquashFS or live environment'
    exit 1
fi

# Create essential directories
mkdir -p /mnt/{etc,usr,var,home,root,tmp}
mkdir -p /mnt/{dev,proc,sys,run}

# Verify installation
test -d /mnt/etc || (echo 'System installation failed - /mnt/etc missing' && exit 1)
test -d /mnt/usr || (echo 'System installation failed - /mnt/usr missing' && exit 1)
)";
    
    executeCommand("bash", QStringList() << "-c" << installScript);
}

void Installer::configureSystem() {
    qDebug() << "[DEBUG] configureSystem() - Starting system configuration";
    qDebug() << "[DEBUG] === BEFORE FINAL SETTINGS ===";
    qDebug() << "[DEBUG] Partitioning mode:" << (config.partitioningMode == PartitioningMode::Manual ? "Manual" : "Automatic");
    qDebug() << "[DEBUG] Filesystem:" << config.filesystem;
    qDebug() << "[DEBUG] System state check:";
    qDebug() << "[DEBUG] - /mnt exists:" << QDir("/mnt").exists();
    qDebug() << "[DEBUG] - /mnt/etc exists:" << QDir("/mnt/etc").exists();
    qDebug() << "[DEBUG] - /mnt/boot exists:" << QDir("/mnt/boot").exists();
    
    // Build complete configuration script
    QStringList configCommands;
    
    // Validate base system is installed
    configCommands << "test -d /mnt/etc || (echo 'Base system not installed - /mnt/etc missing' && exit 1)";
    
    // Write configuration files
    QString langCode = config.language.split(" ").first();
    configCommands << QString("echo '%1' > /mnt/etc/locale.gen").arg(config.language);
    configCommands << QString("echo 'LANG=%1' > /mnt/etc/locale.conf").arg(langCode);
    configCommands << QString("echo '%1' > /mnt/etc/hostname").arg(config.hostname);
    configCommands << QString("echo -e '127.0.0.1\\tlocalhost\\n::1\\t\\tlocalhost\\n127.0.1.1\\t%1' > /mnt/etc/hosts").arg(config.hostname);
    configCommands << QString("echo 'KEYMAP=%1' > /mnt/etc/vconsole.conf").arg(config.keyboardLayout);
    
    // Generate fstab with UUIDs
    configCommands << "genfstab -U /mnt > /mnt/etc/fstab";
    
    // Verify fstab was created properly
    configCommands << "test -s /mnt/etc/fstab || (echo 'Failed to generate fstab' && exit 1)";
    
    // Generate machine ID
    configCommands << "arch-chroot /mnt systemd-machine-id-setup";
    
    // Sync pacman databases if internet available
    configCommands << "if ping -c 1 8.8.8.8 >/dev/null 2>&1; then arch-chroot /mnt pacman -Sy || true; else echo 'No internet - skipping repo sync'; fi";
    
    qDebug() << "[DEBUG] === BEFORE FINAL SETTINGS FROM CONFIG FILE ===";
    
    // Create user
    configCommands << QString("arch-chroot /mnt useradd -m -G wheel,audio,video,optical,storage -s /bin/%1 %2")
                      .arg(config.shell).arg(config.username);
    
    // Set passwords
    configCommands << QString("arch-chroot /mnt bash -c \"echo '%1:%2' | chpasswd\"")
                      .arg(config.username).arg(config.password);
    
    QString rootPass = config.samePassword ? config.password : config.rootPassword;
    configCommands << QString("arch-chroot /mnt bash -c \"echo 'root:%1' | chpasswd\"")
                      .arg(rootPass);
    
    // Enable sudo for wheel group
    configCommands << "arch-chroot /mnt sed -i 's/^# %wheel ALL=(ALL:ALL) ALL/%wheel ALL=(ALL:ALL) ALL/' /etc/sudoers";
    
    // Basic mkinitcpio cleanup and preset configuration
    configCommands << "arch-chroot /mnt rm -f /etc/mkinitcpio.conf.d/archiso.conf";
    
    // Ensure linux.preset exists and is properly configured (overwrite to fix archiso references)
    configCommands << "arch-chroot /mnt bash -c 'cat > /etc/mkinitcpio.d/linux.preset << EOF\nALL_config=\"/etc/mkinitcpio.conf\"\nALL_kver=\"/boot/vmlinuz-linux\"\n\nPRESETS=(\"default\" \"fallback\")\n\ndefault_image=\"/boot/initramfs-linux.img\"\nfallback_image=\"/boot/initramfs-linux-fallback.img\"\nfallback_options=\"-S autodetect\"\nEOF'";
    
    // Generate initial initramfs (REQUIRED before arch7z-system-final)
    configCommands << "echo '[DEBUG] Generating initial initramfs...' && arch-chroot /mnt mkinitcpio -P";
    
    // VM-specific optimizations (kernel parameters only - no package installation)
    if (VMDetection::isVirtualMachine()) {
        QString vmType = VMDetection::getVirtualizationType();
        qDebug() << "[DEBUG] Detected virtual machine:" << vmType << "- applying kernel optimizations only";
        
        // VM-optimized kernel parameters only
        configCommands << "arch-chroot /mnt sed -i 's/GRUB_CMDLINE_LINUX_DEFAULT=\"quiet\"/GRUB_CMDLINE_LINUX_DEFAULT=\"quiet elevator=noop\"/' /etc/default/grub";
    }
    
    qDebug() << "[DEBUG] === EXECUTING BASIC CONFIGURATION ===";
    qDebug() << "[DEBUG] Total basic commands to execute:" << configCommands.size();
    
    // Execute all basic configuration in one script
    QString configScript = configCommands.join(" && ");
    executeCommand("bash", QStringList() << "-c" << configScript);
    
    // Final settings will be executed after bootloader installation
    
    qDebug() << "[DEBUG] === AFTER FINAL SETTINGS FROM CONFIG FILE ===";
}

void Installer::installBootloader() {
    qDebug() << "[DEBUG] === AFTER FINAL SETTINGS - BOOTLOADER ===";
    qDebug() << "[DEBUG] installBootloader() - Starting bootloader installation";
    
    // Get bootloader ID from final-settings.conf
    SettingsParser::loadSettings();
    QString bootloaderId = SettingsParser::getBootloaderId();
    qDebug() << "[DEBUG] Using bootloader ID from config:" << bootloaderId;
    
    // Build complete bootloader installation script
    QStringList bootloaderCommands;
    
    // Validate system is properly configured
    bootloaderCommands << "test -f /mnt/etc/fstab || (echo 'System not properly installed - /mnt/etc/fstab missing' && exit 1)";
    
    // Configure GRUB
    bootloaderCommands << "arch-chroot /mnt sed -i 's/GRUB_CMDLINE_LINUX_DEFAULT=.*/GRUB_CMDLINE_LINUX_DEFAULT=\"quiet\"/' /etc/default/grub";
    
    // Enable os-prober for dual boot detection
    bootloaderCommands << "arch-chroot /mnt sed -i 's/#GRUB_DISABLE_OS_PROBER=false/GRUB_DISABLE_OS_PROBER=false/' /etc/default/grub";
    
    // Set proper timeout
    bootloaderCommands << "arch-chroot /mnt sed -i 's/GRUB_TIMEOUT=.*/GRUB_TIMEOUT=5/' /etc/default/grub";
    
    qDebug() << "[DEBUG] === BOOTLOADER INSTALLATION ===";
    // Install GRUB to EFI using bootloader ID from config
    bootloaderCommands << QString("echo '[DEBUG] Installing GRUB (removable)...' && arch-chroot /mnt grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id=%1 --removable").arg(bootloaderId);
    bootloaderCommands << QString("echo '[DEBUG] Installing GRUB (standard)...' && arch-chroot /mnt grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id=%1").arg(bootloaderId);
    
    
    // Generate GRUB configuration (after GRUB is installed and initramfs exists)
    bootloaderCommands << "echo '[DEBUG] Generating GRUB config...' && arch-chroot /mnt grub-mkconfig -o /boot/grub/grub.cfg";
    
    // Execute all bootloader commands in one script
    QString bootloaderScript = bootloaderCommands.join(" && ");
    executeCommand("bash", QStringList() << "-c" << bootloaderScript);
}

void Installer::runCustomScripts() {
    qDebug() << "[DEBUG] === FINAL SETTINGS EXECUTION (BEFORE CLEANUP) ===";
    
    // Execute final settings AFTER bootloader installation but BEFORE cleanup
    qDebug() << "[DEBUG] === LOADING FINAL SETTINGS FROM CONFIG FILE ===";
    QString configPath = "/usr/share/arch7z-installer/settings/final-settings.conf";
    qDebug() << "[DEBUG] Config file path:" << configPath;
    
    QFile configFile(configPath);
    bool fileExists = configFile.exists();
    qDebug() << "[DEBUG] Config file exists:" << fileExists;
    
    if (fileExists) {
        qint64 fileSize = configFile.size();
        qDebug() << "[DEBUG] Config file size:" << fileSize << "bytes";
        
        if (configFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&configFile);
            QString fileContent = in.readAll();
            configFile.close();
            qDebug() << "[DEBUG] Config file content length:" << fileContent.length();
            qDebug() << "[DEBUG] First 200 chars of config file:" << fileContent.left(200);
            
            QStringList lines = fileContent.split('\n', Qt::SkipEmptyParts);
            qDebug() << "[DEBUG] Config file has" << lines.size() << "non-empty lines";
        } else {
            qDebug() << "[ERROR] Cannot open config file for reading";
        }
    }
    
    // Load and execute final settings from configuration file
    qDebug() << "[DEBUG] Calling SettingsParser::loadSettings()...";
    bool settingsLoaded = SettingsParser::loadSettings();
    qDebug() << "[DEBUG] SettingsParser::loadSettings() returned:" << settingsLoaded;
    
    if (settingsLoaded) {
        qDebug() << "[DEBUG] Calling SettingsParser::getExecutionCommands()...";
        QStringList finalCommands = SettingsParser::getExecutionCommands();
        qDebug() << QString("[DEBUG] SettingsParser::getExecutionCommands() returned %1 commands").arg(finalCommands.size());
        
        if (!finalCommands.isEmpty()) {
            qDebug() << "[DEBUG] Final commands to execute:";
            for (int i = 0; i < finalCommands.size(); ++i) {
                qDebug() << QString("[DEBUG] Command %1: %2").arg(i+1).arg(finalCommands[i]);
            }
            qDebug() << QString("[DEBUG] About to execute %1 final settings commands from config file").arg(finalCommands.size());
            qDebug() << "[DEBUG] Creating execution script...";
            
            QString finalScript = R"(
#!/bin/bash
set +e  # Don't exit on errors

FAILED_COMMANDS=0
TOTAL_COMMANDS=0

)";
            
            for (const QString &cmd : finalCommands) {
                finalScript += QString("echo 'Executing: %1'\n").arg(cmd);
                finalScript += QString("%1\n").arg(cmd);
                finalScript += "if [ $? -ne 0 ]; then\n";
                finalScript += QString("    echo 'WARNING: Command failed: %1'\n").arg(cmd);
                finalScript += "    FAILED_COMMANDS=$((FAILED_COMMANDS + 1))\n";
                finalScript += "fi\n";
                finalScript += "TOTAL_COMMANDS=$((TOTAL_COMMANDS + 1))\n\n";
            }
            
            finalScript += R"(
echo "Final settings completed: $((TOTAL_COMMANDS - FAILED_COMMANDS))/$TOTAL_COMMANDS commands successful"
if [ $FAILED_COMMANDS -gt 0 ]; then
    echo "WARNING: $FAILED_COMMANDS commands failed, but installation can continue"
fi

# Always exit 0 for final settings - failures are not critical
exit 0
)";
            
            qDebug() << "[DEBUG] Final script length:" << finalScript.length();
            qDebug() << "[DEBUG] Executing final settings script...";
            executeCommand("bash", QStringList() << "-c" << finalScript);
            qDebug() << "[DEBUG] Final settings script execution completed";
            
            // Verify critical final settings were applied
            qDebug() << "[DEBUG] === VERIFYING FINAL SETTINGS RESULTS ===";
            
            // Check if plymouth was added to mkinitcpio.conf
            QString plymouthCheck = "grep -q 'plymouth' /mnt/etc/mkinitcpio.conf && echo 'FOUND' || echo 'NOT_FOUND'";
            QProcess plymouthProcess;
            plymouthProcess.start("bash", QStringList() << "-c" << plymouthCheck);
            plymouthProcess.waitForFinished(5000);
            QString plymouthResult = plymouthProcess.readAllStandardOutput().trimmed();
            qDebug() << "[VERIFY] Plymouth in mkinitcpio.conf:" << plymouthResult;
            
            // Check if quiet splash was added to GRUB
            QString grubCheck = "grep 'quiet' /mnt/etc/default/grub | grep 'splash' && echo 'FOUND' || echo 'NOT_FOUND'";
            QProcess grubProcess;
            grubProcess.start("bash", QStringList() << "-c" << grubCheck);
            grubProcess.waitForFinished(5000);
            QString grubResult = grubProcess.readAllStandardOutput().trimmed();
            qDebug() << "[VERIFY] Quiet splash in GRUB:" << (grubResult.contains("FOUND") ? "FOUND" : "NOT_FOUND");
            
            // Check if arch7z-system-final script exists and was executed
            QString scriptCheck = "test -f /mnt/usr/local/bin/arch7z-system-final && echo 'EXISTS' || echo 'MISSING'";
            QProcess scriptProcess;
            scriptProcess.start("bash", QStringList() << "-c" << scriptCheck);
            scriptProcess.waitForFinished(5000);
            QString scriptResult = scriptProcess.readAllStandardOutput().trimmed();
            qDebug() << "[VERIFY] arch7z-system-final script:" << scriptResult;
            
            // Show current mkinitcpio.conf HOOKS line for debugging
            QString hooksCheck = "grep '^HOOKS=' /mnt/etc/mkinitcpio.conf || echo 'HOOKS_LINE_NOT_FOUND'";
            QProcess hooksProcess;
            hooksProcess.start("bash", QStringList() << "-c" << hooksCheck);
            hooksProcess.waitForFinished(5000);
            QString hooksResult = hooksProcess.readAllStandardOutput().trimmed();
            qDebug() << "[VERIFY] Current HOOKS line:" << hooksResult;
            
            // Show current GRUB cmdline for debugging
            QString grubCmdCheck = "grep '^GRUB_CMDLINE_LINUX_DEFAULT=' /mnt/etc/default/grub || echo 'GRUB_CMDLINE_NOT_FOUND'";
            QProcess grubCmdProcess;
            grubCmdProcess.start("bash", QStringList() << "-c" << grubCmdCheck);
            grubCmdProcess.waitForFinished(5000);
            QString grubCmdResult = grubCmdProcess.readAllStandardOutput().trimmed();
            qDebug() << "[VERIFY] Current GRUB cmdline:" << grubCmdResult;
            
            // Check if external scripts directory exists and list available scripts
            QString scriptsCheck = "ls -la /mnt/usr/local/bin/ | grep arch7z || echo 'NO_ARCH7Z_SCRIPTS'";
            QProcess scriptsProcess;
            scriptsProcess.start("bash", QStringList() << "-c" << scriptsCheck);
            scriptsProcess.waitForFinished(5000);
            QString scriptsResult = scriptsProcess.readAllStandardOutput().trimmed();
            qDebug() << "[VERIFY] Available arch7z scripts in /mnt/usr/local/bin/:";
            qDebug() << scriptsResult;
            
            // Check if the scripts are executable
            QString execCheck = "test -x /mnt/usr/local/bin/arch7z-system-final && echo 'EXECUTABLE' || echo 'NOT_EXECUTABLE'";
            QProcess execProcess;
            execProcess.start("bash", QStringList() << "-c" << execCheck);
            execProcess.waitForFinished(5000);
            QString execResult = execProcess.readAllStandardOutput().trimmed();
            qDebug() << "[VERIFY] arch7z-system-final executable:" << execResult;
            
            qDebug() << "[DEBUG] === END VERIFICATION ===";
        } else {
            qDebug() << "[WARNING] No final settings commands found in config file";
        }
    } else {
        qDebug() << "[ERROR] Failed to load final settings from config file";
    }
}



void Installer::cleanup() {
    qDebug() << "[DEBUG] === FINAL CLEANUP ===";
    qDebug() << "[DEBUG] cleanup() - Starting cleanup";
    
    // Emergency memory cleanup FIRST
    QProcess::execute("bash", QStringList() << "-c" << "echo 3 > /proc/sys/vm/drop_caches 2>/dev/null || true");
    QProcess::execute("sync");
    
    QStringList cleanupCommands;
    
    // Kill any processes that might be using /mnt
    cleanupCommands << "fuser -km /mnt 2>/dev/null || true";
    cleanupCommands << "killall -9 rsync cp unsquashfs 2>/dev/null || true";
    
    // Wait for processes to terminate
    cleanupCommands << "sleep 3";
    
    // Force unmount everything
    cleanupCommands << "umount -f /mnt/boot/efi 2>/dev/null || true";
    cleanupCommands << "umount -f /mnt/boot 2>/dev/null || true";
    cleanupCommands << "umount -f /mnt/proc 2>/dev/null || true";
    cleanupCommands << "umount -f /mnt/sys 2>/dev/null || true";
    cleanupCommands << "umount -f /mnt/dev 2>/dev/null || true";
    cleanupCommands << "umount -f /mnt 2>/dev/null || true";
    
    // Clean up loop devices and temp files
    cleanupCommands << "losetup -D 2>/dev/null || true";
    cleanupCommands << "rm -rf /tmp/squashfs-root 2>/dev/null || true";
    
    // Disable swap
    if (config.enableSwap) {
        QString swapPartition;
        if (config.partitioningMode == PartitioningMode::Manual) {
            swapPartition = config.swapPartition;
        } else {
            swapPartition = getPartitionName(config.selectedDisk, 2);
        }
        if (!swapPartition.isEmpty()) {
            cleanupCommands << QString("swapoff %1 2>/dev/null || true").arg(swapPartition);
        }
    }
    
    // Final memory cleanup
    cleanupCommands << "sync";
    cleanupCommands << "echo 3 > /proc/sys/vm/drop_caches 2>/dev/null || true";
    
    QString cleanupScript = cleanupCommands.join(" && ");
    executeCommand("bash", QStringList() << "-c" << cleanupScript);
}

void Installer::executeCommand(const QString &command, const QStringList &args) {
    if (currentProcess) {
        currentProcess->deleteLater();
    }
    
    currentProcess = new QProcess(this);
    connect(currentProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &Installer::onProcessFinished);
    connect(currentProcess, &QProcess::errorOccurred, [this](QProcess::ProcessError error) {
        QString errorMsg = QString("Process error: %1").arg(currentProcess->errorString());
        qDebug() << "[ERROR]" << errorMsg;
        emit installationFinished(false, errorMsg);
    });
    
    QString fullCommand = command + " " + args.join(" ");
    qDebug() << "[EXEC] Starting:" << fullCommand;
    
    // Use pkexec for privileged operations
    if (command == "bash" && !args.isEmpty() && args.first() == "-c") {
        // Check if pkexec is available, fallback to sudo if needed
        if (QProcess::execute("which", QStringList() << "pkexec") == 0) {
            currentProcess->start("pkexec", QStringList() << command << args);
        } else if (QProcess::execute("which", QStringList() << "sudo") == 0) {
            currentProcess->start("sudo", QStringList() << command << args);
        } else {
            currentProcess->start(command, args);
        }
    } else {
        currentProcess->start(command, args);
    }
    
    qDebug() << "[DEBUG] Command started, waiting for completion...";
}

void Installer::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    QString stdOut = currentProcess->readAllStandardOutput();
    QString stdErr = currentProcess->readAllStandardError();
    QString program = currentProcess->program();
    QStringList arguments = currentProcess->arguments();
    
    qDebug() << QString("[RESULT] Step: %1/%2 - %3")
                .arg(currentStep+1).arg(totalSteps).arg(installSteps[currentStep]);
    qDebug() << QString("[RESULT] Command: %1 %2").arg(program).arg(arguments.join(" "));
    qDebug() << QString("[RESULT] Exit code: %1, Status: %2")
                .arg(exitCode)
                .arg(exitStatus == QProcess::NormalExit ? "Normal" : "Crashed");
    
    if (!stdOut.isEmpty()) {
        qDebug() << "[STDOUT]" << stdOut.left(500) + (stdOut.length() > 500 ? "..." : "");
    }
    if (!stdErr.isEmpty()) {
        qDebug() << "[STDERR]" << stdErr.left(500) + (stdErr.length() > 500 ? "..." : "");
    }
    
    if (exitCode != 0) {
        QString errorMsg = QString("FAILED: %1\n\nCommand: %2 %3\nExit Code: %4\n\nError Output:\n%5\n\nStandard Output:\n%6")
                          .arg(installSteps[currentStep])
                          .arg(program)
                          .arg(arguments.join(" "))
                          .arg(exitCode)
                          .arg(stdErr.isEmpty() ? "(none)" : stdErr)
                          .arg(stdOut.isEmpty() ? "(none)" : stdOut);
        qDebug() << "[FATAL]" << errorMsg;
        
        terminateInstallation();
        emit installationFinished(false, errorMsg);
        return;
    }
    
    qDebug() << QString("[SUCCESS] Step %1 completed").arg(installSteps[currentStep]);
    
    // Proactive memory cleanup after each step
    if (currentStep == 3) { // After base system installation
        qDebug() << "[DEBUG] Performing memory cleanup after base system installation";
        QProcess::execute("sync");
        QProcess::execute("bash", QStringList() << "-c" << "echo 1 > /proc/sys/vm/drop_caches 2>/dev/null || true");
    }
    
    // Check memory status
    QProcess memCheck;
    memCheck.start("bash", QStringList() << "-c" << "awk '/MemAvailable/ {print $2}' /proc/meminfo");
    memCheck.waitForFinished(2000);
    QString memAvailable = memCheck.readAllStandardOutput().trimmed();
    qDebug() << "[MEMORY] Available:" << memAvailable << "KB";
    
    // Emergency cleanup if memory is low
    if (!memAvailable.isEmpty() && memAvailable.toInt() < 512000) {
        qDebug() << "[WARNING] Low memory detected, forcing cleanup";
        QProcess::execute("bash", QStringList() << "-c" << "echo 3 > /proc/sys/vm/drop_caches 2>/dev/null || true");
    }
    
    currentStep++;
    
    if (currentStep >= totalSteps) {
        qDebug() << "[DEBUG] Installation completed successfully!";
        emit installationFinished(true, "Installation completed successfully!");
        return;
    }
    
    progressTimer->start(500);
}

void Installer::updateProgress(int percentage, const QString &message) {
    emit progressChanged(percentage, message);
}

void Installer::writeFile(const QString &path, const QString &content) {
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << content;
        file.close();
    }
}

void Installer::appendFile(const QString &path, const QString &content) {
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << content;
        file.close();
    }
}

void Installer::terminateInstallation() {
    qDebug() << "[DEBUG] terminateInstallation() - Cleaning up failed installation";
    
    // Stop the progress timer
    if (progressTimer && progressTimer->isActive()) {
        progressTimer->stop();
    }
    
    // Kill any running process
    if (currentProcess && currentProcess->state() != QProcess::NotRunning) {
        qDebug() << "[DEBUG] Terminating running process:" << currentProcess->program();
        currentProcess->kill();
        currentProcess->waitForFinished(5000);
    }
    
    // Emergency cleanup - try to unmount anything that might be mounted
    QProcess cleanup;
    cleanup.start("bash", QStringList() << "-c" << "umount -R /mnt 2>/dev/null || true");
    cleanup.waitForFinished(10000);
    
    // Disable swap if it was enabled
    if (config.enableSwap) {
        QString swapPartition = getPartitionName(config.selectedDisk, 2);
        QProcess swapOff;
        swapOff.start("swapoff", QStringList() << swapPartition);
        swapOff.waitForFinished(5000);
    }
    
    qDebug() << "[DEBUG] terminateInstallation() - Cleanup completed";
}