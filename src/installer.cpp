#include "installer.h"
#include "vmdetection.h"
#include <QDebug>
#include <QDir>
#include <QTextStream>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QRegularExpression>

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
    partitionCommands << "sleep 1";
    
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
    
    QString squashfsPath = "/run/archiso/bootmnt/arch/x86_64/airootfs.sfs";
    qDebug() << "[DEBUG] installBaseSystem() - Looking for SquashFS at" << squashfsPath;
    
    // Build complete installation script with proper validation
    QStringList installCommands;
    
    // Validate that /mnt exists and is mounted
    installCommands << "test -d /mnt || (echo '/mnt directory does not exist' && exit 1)";
    installCommands << "mountpoint -q /mnt || (echo '/mnt is not mounted' && exit 1)";
    installCommands << "test -d /mnt/boot/efi || (echo '/mnt/boot/efi is not mounted' && exit 1)";
    installCommands << "mountpoint -q /mnt/boot/efi || (echo '/mnt/boot/efi is not mounted' && exit 1)";
    
    // Validate SquashFS exists
    installCommands << QString("test -f %1 || (echo 'SquashFS not found: %1' && exit 1)").arg(squashfsPath);
    
    // Create temporary mount point
    installCommands << "mkdir -p /tmp/squashfs-root";
    
    // Mount SquashFS
    installCommands << QString("mount -t squashfs -o loop %1 /tmp/squashfs-root").arg(squashfsPath);
    
    // Extract system to mounted target
    installCommands << "rsync -aHAXS --numeric-ids --exclude=/dev/* --exclude=/proc/* --exclude=/sys/* --exclude=/tmp/* --exclude=/run/* --exclude=/mnt/* --exclude=/media/* --exclude=/lost+found /tmp/squashfs-root/ /mnt/";
    
    // Copy kernel if it exists
    QString kernelPath = "/run/archiso/bootmnt/arch/boot/x86_64/vmlinuz-linux";
    installCommands << "mkdir -p /mnt/boot";
    installCommands << QString("test -f %1 && cp %1 /mnt/boot/vmlinuz-linux || echo 'Kernel not found, skipping'").arg(kernelPath);
    
    // Cleanup
    installCommands << "umount /tmp/squashfs-root";
    installCommands << "rmdir /tmp/squashfs-root";
    
    // Verify installation was successful
    installCommands << "test -d /mnt/etc || (echo 'System installation failed - /mnt/etc missing' && exit 1)";
    installCommands << "test -d /mnt/usr || (echo 'System installation failed - /mnt/usr missing' && exit 1)";
    
    // Execute complete installation in one script
    QString installScript = installCommands.join(" && ");
    executeCommand("bash", QStringList() << "-c" << installScript);
}

void Installer::configureSystem() {
    qDebug() << "[DEBUG] configureSystem() - Starting system configuration";
    
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
    
    // Initialize pacman keys
    configCommands << "arch-chroot /mnt pacman-key --init";
    configCommands << "arch-chroot /mnt pacman-key --populate archlinux";
    configCommands << "arch-chroot /mnt pacman-key --populate chaotic";
    
    // Configure locale
    configCommands << "arch-chroot /mnt locale-gen";
    
    // Configure timezone and hardware clock
    configCommands << QString("arch-chroot /mnt ln -sf /usr/share/zoneinfo/%1 /etc/localtime").arg(config.timezone);
    configCommands << "arch-chroot /mnt hwclock --systohc";
    
    // Remove liveuser (ignore errors if user doesn't exist)
    configCommands << "arch-chroot /mnt userdel -r liveuser 2>/dev/null || true";
    
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
    
    // Fix mkinitcpio configuration for installed system
    configCommands << "arch-chroot /mnt rm -f /etc/mkinitcpio.conf.d/archiso.conf";
    configCommands << "arch-chroot /mnt rm -f /etc/mkinitcpio.d/linux.preset";
    configCommands << "arch-chroot /mnt sed -i 's/^HOOKS=.*/HOOKS=(base udev autodetect microcode modconf kms keyboard keymap consolefont block filesystems fsck)/' /etc/mkinitcpio.conf";
    
    // Create proper preset file for installed system
    configCommands << "arch-chroot /mnt bash -c 'cat > /etc/mkinitcpio.d/linux.preset << EOF\nALL_config=\"/etc/mkinitcpio.conf\"\nALL_kver=\"/boot/vmlinuz-linux\"\n\nPRESETS=(\"default\" \"fallback\")\n\ndefault_image=\"/boot/initramfs-linux.img\"\nfallback_image=\"/boot/initramfs-linux-fallback.img\"\nfallback_options=\"-S autodetect\"\nEOF'";
    
    // Configure initramfs with specific kernel
    configCommands << "arch-chroot /mnt mkinitcpio -p linux";
    
    // Enable NetworkManager
    configCommands << "arch-chroot /mnt systemctl enable NetworkManager";
    
    // VM-specific optimizations
    if (VMDetection::isVirtualMachine()) {
        QString vmType = VMDetection::getVirtualizationType();
        qDebug() << "[DEBUG] Detected virtual machine:" << vmType;
        
        // Install VM guest tools
        if (vmType.contains("vmware")) {
            configCommands << "arch-chroot /mnt pacman -S --noconfirm open-vm-tools";
            configCommands << "arch-chroot /mnt systemctl enable vmtoolsd";
        } else if (vmType.contains("virtualbox")) {
            configCommands << "arch-chroot /mnt pacman -S --noconfirm virtualbox-guest-utils";
            configCommands << "arch-chroot /mnt systemctl enable vboxservice";
        } else if (vmType.contains("qemu") || vmType.contains("kvm")) {
            configCommands << "arch-chroot /mnt pacman -S --noconfirm qemu-guest-agent";
            configCommands << "arch-chroot /mnt systemctl enable qemu-guest-agent";
        }
        
        // VM-optimized kernel parameters
        configCommands << "arch-chroot /mnt sed -i 's/GRUB_CMDLINE_LINUX_DEFAULT=\"quiet\"/GRUB_CMDLINE_LINUX_DEFAULT=\"quiet elevator=noop\"/' /etc/default/grub";
    }
    
    // Execute all configuration in one script
    QString configScript = configCommands.join(" && ");
    executeCommand("bash", QStringList() << "-c" << configScript);
}

void Installer::installBootloader() {
    qDebug() << "[DEBUG] installBootloader() - Starting bootloader installation";
    
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
    
    // Install GRUB to EFI
    bootloaderCommands << "arch-chroot /mnt grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id=arch7z --removable";
    bootloaderCommands << "arch-chroot /mnt grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id=arch7z";
    
    // Generate GRUB configuration
    bootloaderCommands << "arch-chroot /mnt grub-mkconfig -o /boot/grub/grub.cfg";
    
    // Execute all bootloader commands in one script
    QString bootloaderScript = bootloaderCommands.join(" && ");
    executeCommand("bash", QStringList() << "-c" << bootloaderScript);
}

void Installer::runCustomScripts() {
    // Skip custom scripts for now - they may not exist in the live environment
    qDebug() << "[DEBUG] Skipping custom scripts - not implemented yet";
    
    // Since we're not executing any command, manually advance to next step
    qDebug() << QString("[SUCCESS] Step %1 completed").arg(installSteps[currentStep]);
    currentStep++;
    
    // Check if installation is complete
    if (currentStep >= totalSteps) {
        qDebug() << "[DEBUG] Installation completed successfully!";
        emit installationFinished(true, "Installation completed successfully!");
        return;
    }
    
    // Continue with next step
    progressTimer->start(500);
}

void Installer::cleanup() {
    qDebug() << "[DEBUG] cleanup() - Starting cleanup";
    
    QStringList cleanupCommands;
    
    // Unmount partitions
    cleanupCommands << "umount -R /mnt";
    
    // Disable swap if it was enabled
    if (config.enableSwap) {
        QString swapPartition;
        if (config.partitioningMode == PartitioningMode::Manual) {
            swapPartition = config.swapPartition;
        } else {
            swapPartition = getPartitionName(config.selectedDisk, 2);
        }
        if (!swapPartition.isEmpty()) {
            cleanupCommands << QString("swapoff %1").arg(swapPartition);
        }
    }
    
    // Execute all cleanup commands in one script
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
    
    // Start the process asynchronously - no synchronous waits
    currentProcess->start(command, args);
    
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
        
        // Terminate any running processes and cleanup
        terminateInstallation();
        emit installationFinished(false, errorMsg);
        return;
    }
    
    qDebug() << QString("[SUCCESS] Step %1 completed").arg(installSteps[currentStep]);
    
    // Move to next step
    currentStep++;
    
    // Check if installation is complete
    if (currentStep >= totalSteps) {
        qDebug() << "[DEBUG] Installation completed successfully!";
        emit installationFinished(true, "Installation completed successfully!");
        return;
    }
    
    // Continue with next step
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