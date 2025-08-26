#include "advancedpartition.h"
#include "userconfig.h"
#include "installconfig.h"
#include "vmdetection.h"
#include <QApplication>
#include <QMessageBox>
#include <QHeaderView>
#include <QSplitter>
#include <QGridLayout>
#include <QRegularExpression>

extern InstallConfig g_installConfig;

AdvancedPartitionWindow::AdvancedPartitionWindow(QWidget *parent) 
    : QMainWindow(parent), selectedRow(-1) {
    setupUI();
    refreshPartitions();
}

void AdvancedPartitionWindow::setupUI() {
    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(40, 40, 40, 40);
    
    // Title
    QLabel *title = new QLabel("Arch7z Installer", this);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-size: 28px; font-weight: 300; color: #18e8ec; margin-bottom: 10px;");
    
    QLabel *subtitle = new QLabel("Advanced Partition Setup", this);
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setStyleSheet("font-size: 14px; color: #888; margin-bottom: 20px;");
    
    mainLayout->addWidget(title);
    mainLayout->addWidget(subtitle);
    
    // Drive selection
    QHBoxLayout *driveLayout = new QHBoxLayout();
    QLabel *driveLabel = new QLabel("Select Drive:", this);
    driveLabel->setStyleSheet("color: white; font-size: 14px;");
    driveCombo = new QComboBox(this);
    driveCombo->setStyleSheet("QComboBox { background-color: #444; color: white; padding: 5px; }");
    driveLayout->addWidget(driveLabel);
    driveLayout->addWidget(driveCombo);
    driveLayout->addStretch();
    mainLayout->addLayout(driveLayout);
    
    // Tool buttons
    QHBoxLayout *toolLayout = new QHBoxLayout();
    refreshButton = new QPushButton("Refresh", this);
    gpartedButton = new QPushButton("Open GParted", this);
    
    QString toolButtonStyle = 
        "QPushButton {"
        "  background-color: #444;"
        "  color: white;"
        "  border: none;"
        "  padding: 8px 16px;"
        "  border-radius: 4px;"
        "  font-size: 12px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #555;"
        "}";
    
    refreshButton->setStyleSheet(toolButtonStyle);
    gpartedButton->setStyleSheet(toolButtonStyle);
    
    toolLayout->addWidget(refreshButton);
    toolLayout->addWidget(gpartedButton);
    toolLayout->addStretch();
    mainLayout->addLayout(toolLayout);
    
    // Main content splitter
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    
    // Partition table
    partitionTable = new QTableWidget(this);
    partitionTable->setColumnCount(6);
    partitionTable->setHorizontalHeaderLabels(QStringList() << "Device" << "Label" << "Size" << "Filesystem" << "Mount Point" << "Assignment");
    partitionTable->setColumnWidth(0, 100);
    partitionTable->setColumnWidth(1, 120);
    partitionTable->setColumnWidth(2, 80);
    partitionTable->setColumnWidth(3, 100);
    partitionTable->setColumnWidth(4, 100);
    partitionTable->setColumnWidth(5, 100);
    partitionTable->horizontalHeader()->setStretchLastSection(true);
    partitionTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    partitionTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    partitionTable->setStyleSheet(
        "QTableWidget {"
        "  background-color: #1a1a2e;"
        "  border: 1px solid #18e8ec;"
        "  border-radius: 6px;"
        "  color: white;"
        "  font-size: 14px;"
        "}"
        "QTableWidget::item {"
        "  padding: 10px;"
        "  border-bottom: 1px solid #333;"
        "}"
        "QHeaderView::section {"
        "  background-color: #18e8ec;"
        "  color: black;"
        "  padding: 8px;"
        "  border: none;"
        "  font-weight: bold;"
        "}"
    );
    
    // Assignment panel
    assignmentGroup = new QGroupBox("Partition Assignment", this);
    assignmentGroup->setStyleSheet(
        "QGroupBox {"
        "  color: white;"
        "  font-size: 16px;"
        "  font-weight: bold;"
        "  border: 2px solid #18e8ec;"
        "  border-radius: 8px;"
        "  margin-top: 10px;"
        "  padding-top: 10px;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin;"
        "  left: 10px;"
        "  padding: 0 5px 0 5px;"
        "}"
    );
    
    QVBoxLayout *assignLayout = new QVBoxLayout(assignmentGroup);
    
    // Boot/EFI section
    QLabel *bootLabel = new QLabel("Boot/EFI Partition (min 300MB):", this);
    bootLabel->setStyleSheet("color: white; font-size: 14px; margin-bottom: 5px;");
    bootFormatCombo = new QComboBox(this);
    bootFormatCombo->addItem("FAT32");
    bootFormatCombo->setStyleSheet("QComboBox { background-color: #444; color: white; padding: 5px; }");
    setBootButton = new QPushButton("Set as Boot/EFI", this);
    
    // Root section
    QLabel *rootLabel = new QLabel("Root Partition:", this);
    rootLabel->setStyleSheet("color: white; font-size: 14px; margin-bottom: 5px;");
    rootFormatCombo = new QComboBox(this);
    rootFormatCombo->addItems(QStringList() << "Btrfs" << "Ext4");
    rootFormatCombo->setStyleSheet("QComboBox { background-color: #444; color: white; padding: 5px; }");
    setRootButton = new QPushButton("Set as Root", this);
    
    // Swap section
    setSwapButton = new QPushButton("Set as Swap", this);
    clearButton = new QPushButton("Clear Assignment", this);
    
    QString assignButtonStyle = 
        "QPushButton {"
        "  background-color: #18e8ec;"
        "  color: black;"
        "  border: none;"
        "  padding: 8px 16px;"
        "  border-radius: 4px;"
        "  font-size: 12px;"
        "  margin: 2px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #0ea5a8;"
        "}"
        "QPushButton:disabled {"
        "  background-color: #666;"
        "  color: #999;"
        "}";
    
    setBootButton->setStyleSheet(assignButtonStyle);
    setRootButton->setStyleSheet(assignButtonStyle);
    setSwapButton->setStyleSheet(assignButtonStyle);
    clearButton->setStyleSheet(assignButtonStyle);
    
    assignLayout->addWidget(bootLabel);
    assignLayout->addWidget(bootFormatCombo);
    assignLayout->addWidget(setBootButton);
    assignLayout->addSpacing(10);
    assignLayout->addWidget(rootLabel);
    assignLayout->addWidget(rootFormatCombo);
    assignLayout->addWidget(setRootButton);
    assignLayout->addSpacing(10);
    assignLayout->addWidget(setSwapButton);
    assignLayout->addSpacing(10);
    assignLayout->addWidget(clearButton);
    assignLayout->addStretch();
    
    splitter->addWidget(partitionTable);
    splitter->addWidget(assignmentGroup);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 1);
    
    mainLayout->addWidget(splitter);
    
    // Navigation buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    backButton = new QPushButton("Back", this);
    backButton->setStyleSheet(
        "QPushButton {"
        "  background-color: #444;"
        "  color: white;"
        "  border: none;"
        "  padding: 12px 24px;"
        "  border-radius: 6px;"
        "  font-size: 14px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #555;"
        "}"
    );
    
    continueButton = new QPushButton("Continue", this);
    continueButton->setStyleSheet(
        "QPushButton {"
        "  background-color: #18e8ec;"
        "  color: black;"
        "  border: none;"
        "  padding: 12px 24px;"
        "  border-radius: 6px;"
        "  font-size: 14px;"
        "  font-weight: 500;"
        "}"
        "QPushButton:hover {"
        "  background-color: #0ea5a8;"
        "}"
        "QPushButton:disabled {"
        "  background-color: #666;"
        "  color: #999;"
        "}"
    );
    
    buttonLayout->addWidget(backButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(continueButton);
    mainLayout->addLayout(buttonLayout);
    
    // Connect signals
    connect(refreshButton, &QPushButton::clicked, this, &AdvancedPartitionWindow::onRefresh);
    connect(gpartedButton, &QPushButton::clicked, this, &AdvancedPartitionWindow::onOpenGParted);
    connect(driveCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AdvancedPartitionWindow::onDriveChanged);
    connect(partitionTable, &QTableWidget::itemSelectionChanged, this, &AdvancedPartitionWindow::onPartitionSelected);
    connect(setBootButton, &QPushButton::clicked, this, &AdvancedPartitionWindow::onSetBootPartition);
    connect(setRootButton, &QPushButton::clicked, this, &AdvancedPartitionWindow::onSetRootPartition);
    connect(setSwapButton, &QPushButton::clicked, this, &AdvancedPartitionWindow::onSetSwapPartition);
    connect(clearButton, &QPushButton::clicked, this, &AdvancedPartitionWindow::onClearPartition);
    connect(continueButton, &QPushButton::clicked, this, &AdvancedPartitionWindow::onContinue);
    connect(backButton, &QPushButton::clicked, this, &AdvancedPartitionWindow::onBack);
    
    // Window styling
    setStyleSheet(
        "QMainWindow {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1, "
        "    stop:0 #1a1a2e, stop:0.5 #16213e, stop:1 #0f0f23);"
        "}"
        "QLabel {"
        "  color: white;"
        "  font-size: 14px;"
        "}"
    );
    
    updateContinueButton();
}

void AdvancedPartitionWindow::refreshPartitions() {
    drivePartitions.clear();
    
    QProcess lsblk;
    lsblk.start("lsblk", QStringList() << "-P" << "-o" << "NAME,LABEL,SIZE,FSTYPE,MOUNTPOINT,TYPE");
    lsblk.waitForFinished(10000);
    
    if (lsblk.exitCode() == 0) {
        QString output = lsblk.readAllStandardOutput();
        QStringList lines = output.split('\n', Qt::SkipEmptyParts);
        
        for (const QString &line : lines) {
            if (line.contains("TYPE=\"part\"")) {
                PartitionInfo partition;
                
                // Parse NAME
                QRegularExpression nameRegex("NAME=\"([^\"]*)\"");
                QRegularExpressionMatch nameMatch = nameRegex.match(line);
                QString deviceName;
                if (nameMatch.hasMatch()) {
                    deviceName = nameMatch.captured(1);
                    partition.device = "/dev/" + deviceName;
                }
                
                // Parse LABEL
                QRegularExpression labelRegex("LABEL=\"([^\"]*)\"");
                QRegularExpressionMatch labelMatch = labelRegex.match(line);
                if (labelMatch.hasMatch() && !labelMatch.captured(1).isEmpty()) {
                    partition.label = labelMatch.captured(1);
                } else {
                    partition.label = deviceName;
                }
                
                // Parse SIZE
                QRegularExpression sizeRegex("SIZE=\"([^\"]*)\"");
                QRegularExpressionMatch sizeMatch = sizeRegex.match(line);
                if (sizeMatch.hasMatch()) {
                    partition.size = sizeMatch.captured(1);
                }
                
                // Parse FSTYPE
                QRegularExpression fstypeRegex("FSTYPE=\"([^\"]*)\"");
                QRegularExpressionMatch fstypeMatch = fstypeRegex.match(line);
                if (fstypeMatch.hasMatch() && !fstypeMatch.captured(1).isEmpty()) {
                    partition.filesystem = fstypeMatch.captured(1);
                } else {
                    partition.filesystem = "unformatted";
                }
                
                // Parse MOUNTPOINT
                QRegularExpression mountRegex("MOUNTPOINT=\"([^\"]*)\"");
                QRegularExpressionMatch mountMatch = mountRegex.match(line);
                if (mountMatch.hasMatch() && !mountMatch.captured(1).isEmpty()) {
                    partition.mountpoint = mountMatch.captured(1);
                } else {
                    partition.mountpoint = "";
                }
                
                partition.isAssigned = false;
                partition.assignedAs = "";
                
                if (!partition.device.isEmpty()) {
                    QString drive = deviceName;
                    // Extract drive name (remove partition number)
                    // Handle nvme (nvme0n1p1 -> nvme0n1), mmc (mmcblk0p1 -> mmcblk0), regular (sda1 -> sda)
                    if (deviceName.contains("nvme") && deviceName.contains("p")) {
                        drive = deviceName.left(deviceName.lastIndexOf('p'));
                    } else if (deviceName.contains("mmcblk") && deviceName.contains("p")) {
                        drive = deviceName.left(deviceName.lastIndexOf('p'));
                    } else {
                        // Regular drives like sda1 -> sda
                        QRegularExpression driveRegex("^([a-zA-Z]+)([0-9]+)$");
                        QRegularExpressionMatch driveMatch = driveRegex.match(deviceName);
                        if (driveMatch.hasMatch()) {
                            drive = driveMatch.captured(1);
                        }
                    }
                    drivePartitions[drive].append(partition);
                }
            }
        }
    }
    
    populateDriveCombo();
}

void AdvancedPartitionWindow::populateDriveCombo() {
    driveCombo->clear();
    for (auto it = drivePartitions.begin(); it != drivePartitions.end(); ++it) {
        driveCombo->addItem("/dev/" + it.key());
    }
    if (driveCombo->count() > 0) {
        QString selectedDrive = driveCombo->currentText();
        currentDrive = selectedDrive.startsWith("/dev/") ? selectedDrive.mid(5) : selectedDrive;
        updatePartitionTable();
    }
}

void AdvancedPartitionWindow::onDriveChanged() {
    QString selectedDrive = driveCombo->currentText();
    currentDrive = selectedDrive.startsWith("/dev/") ? selectedDrive.mid(5) : selectedDrive;
    selectedRow = -1;
    partitionTable->clearSelection();
    updatePartitionTable();
    updateContinueButton();
    
    // Update button states
    setBootButton->setEnabled(false);
    setRootButton->setEnabled(false);
    setSwapButton->setEnabled(false);
    clearButton->setEnabled(false);
}

void AdvancedPartitionWindow::updatePartitionTable() {
    if (currentDrive.isEmpty() || !drivePartitions.contains(currentDrive)) {
        partitionTable->setRowCount(0);
        return;
    }
    
    partitions = drivePartitions[currentDrive];
    partitionTable->setRowCount(partitions.size());
    
    for (int i = 0; i < partitions.size(); ++i) {
        const PartitionInfo &p = partitions[i];
        partitionTable->setItem(i, 0, new QTableWidgetItem(p.device));
        partitionTable->setItem(i, 1, new QTableWidgetItem(p.label));
        partitionTable->setItem(i, 2, new QTableWidgetItem(p.size));
        partitionTable->setItem(i, 3, new QTableWidgetItem(p.filesystem));
        partitionTable->setItem(i, 4, new QTableWidgetItem(p.mountpoint));
        
        QTableWidgetItem *assignItem = new QTableWidgetItem();
        if (!p.assignedAs.isEmpty()) {
            assignItem->setText(p.assignedAs);
            assignItem->setBackground(QBrush(QColor(24, 232, 236, 100)));
            
            // Update global assignment variables
            if (p.assignedAs == "boot") {
                bootPartition = p.device;
            } else if (p.assignedAs == "root") {
                rootPartition = p.device;
            } else if (p.assignedAs == "swap") {
                swapPartition = p.device;
            }
        } else {
            assignItem->setText("");
        }
        partitionTable->setItem(i, 5, assignItem);
    }
}

void AdvancedPartitionWindow::onRefresh() {
    // Clear current assignments
    bootPartition.clear();
    rootPartition.clear();
    swapPartition.clear();
    selectedRow = -1;
    
    // Refresh partition data
    refreshPartitions();
    
    // Update UI
    updateContinueButton();
    
    // Clear selection
    partitionTable->clearSelection();
    setBootButton->setEnabled(false);
    setRootButton->setEnabled(false);
    setSwapButton->setEnabled(false);
    clearButton->setEnabled(false);
}

void AdvancedPartitionWindow::onOpenGParted() {
    QProcess::startDetached("gparted");
}



void AdvancedPartitionWindow::onPartitionSelected() {
    selectedRow = partitionTable->currentRow();
    bool hasSelection = selectedRow >= 0 && selectedRow < partitions.size();
    
    setBootButton->setEnabled(hasSelection);
    setRootButton->setEnabled(hasSelection);
    setSwapButton->setEnabled(hasSelection);
    clearButton->setEnabled(hasSelection && partitions[selectedRow].isAssigned);
}

void AdvancedPartitionWindow::onSetBootPartition() {
    if (selectedRow < 0 || selectedRow >= partitions.size()) return;
    
    PartitionInfo &p = partitions[selectedRow];
    
    // Check minimum size (300MB)
    QString sizeStr = p.size.toLower();
    double sizeMB = 0;
    if (sizeStr.contains("g")) {
        sizeMB = sizeStr.remove("g").toDouble() * 1024;
    } else if (sizeStr.contains("m")) {
        sizeMB = sizeStr.remove("m").toDouble();
    }
    
    if (sizeMB < 300) {
        QMessageBox::warning(this, "Invalid Size", "Boot/EFI partition must be at least 300MB.");
        return;
    }
    
    // Clear previous boot assignment in all drives
    for (auto &driveList : drivePartitions) {
        for (auto &part : driveList) {
            if (part.assignedAs == "boot") {
                part.assignedAs = "";
                part.isAssigned = false;
            }
        }
    }
    
    // Clear previous boot assignment in current partitions
    for (auto &part : partitions) {
        if (part.assignedAs == "boot") {
            part.assignedAs = "";
            part.isAssigned = false;
        }
    }
    
    p.assignedAs = "boot";
    p.isAssigned = true;
    bootPartition = p.device;
    
    // Update the partition in drivePartitions map
    if (drivePartitions.contains(currentDrive)) {
        for (auto &part : drivePartitions[currentDrive]) {
            if (part.device == p.device) {
                part.assignedAs = "boot";
                part.isAssigned = true;
                break;
            }
        }
    }
    
    updatePartitionTable();
    updateContinueButton();
}

void AdvancedPartitionWindow::onSetRootPartition() {
    if (selectedRow < 0 || selectedRow >= partitions.size()) return;
    
    PartitionInfo &p = partitions[selectedRow];
    
    // Clear previous root assignment in all drives
    for (auto &driveList : drivePartitions) {
        for (auto &part : driveList) {
            if (part.assignedAs == "root") {
                part.assignedAs = "";
                part.isAssigned = false;
            }
        }
    }
    
    // Clear previous root assignment in current partitions
    for (auto &part : partitions) {
        if (part.assignedAs == "root") {
            part.assignedAs = "";
            part.isAssigned = false;
        }
    }
    
    p.assignedAs = "root";
    p.isAssigned = true;
    rootPartition = p.device;
    
    // Update the partition in drivePartitions map
    if (drivePartitions.contains(currentDrive)) {
        for (auto &part : drivePartitions[currentDrive]) {
            if (part.device == p.device) {
                part.assignedAs = "root";
                part.isAssigned = true;
                break;
            }
        }
    }
    
    updatePartitionTable();
    updateContinueButton();
}

void AdvancedPartitionWindow::onSetSwapPartition() {
    if (selectedRow < 0 || selectedRow >= partitions.size()) return;
    
    PartitionInfo &p = partitions[selectedRow];
    
    // Clear previous swap assignment in all drives
    for (auto &driveList : drivePartitions) {
        for (auto &part : driveList) {
            if (part.assignedAs == "swap") {
                part.assignedAs = "";
                part.isAssigned = false;
            }
        }
    }
    
    // Clear previous swap assignment in current partitions
    for (auto &part : partitions) {
        if (part.assignedAs == "swap") {
            part.assignedAs = "";
            part.isAssigned = false;
        }
    }
    
    p.assignedAs = "swap";
    p.isAssigned = true;
    swapPartition = p.device;
    
    // Update the partition in drivePartitions map
    if (drivePartitions.contains(currentDrive)) {
        for (auto &part : drivePartitions[currentDrive]) {
            if (part.device == p.device) {
                part.assignedAs = "swap";
                part.isAssigned = true;
                break;
            }
        }
    }
    
    updatePartitionTable();
    updateContinueButton();
}

void AdvancedPartitionWindow::onClearPartition() {
    if (selectedRow < 0 || selectedRow >= partitions.size()) return;
    
    PartitionInfo &p = partitions[selectedRow];
    
    if (p.assignedAs == "boot") bootPartition.clear();
    else if (p.assignedAs == "root") rootPartition.clear();
    else if (p.assignedAs == "swap") swapPartition.clear();
    
    p.assignedAs = "";
    p.isAssigned = false;
    
    // Update the partition in drivePartitions map
    if (drivePartitions.contains(currentDrive)) {
        for (auto &part : drivePartitions[currentDrive]) {
            if (part.device == p.device) {
                part.assignedAs = "";
                part.isAssigned = false;
                break;
            }
        }
    }
    
    updatePartitionTable();
    updateContinueButton();
    
    // Update button states
    onPartitionSelected();
}

void AdvancedPartitionWindow::updateContinueButton() {
    continueButton->setEnabled(validatePartitions());
}

bool AdvancedPartitionWindow::validatePartitions() {
    if (VMDetection::isVirtualMachine()) {
        return !rootPartition.isEmpty(); // VMs only need root partition
    }
    return !bootPartition.isEmpty() && !rootPartition.isEmpty(); // Physical needs both
}

void AdvancedPartitionWindow::onContinue() {
    if (!validatePartitions()) {
        QString message = VMDetection::isVirtualMachine() ? 
            "You must assign a root partition." : 
            "You must assign both boot/EFI and root partitions.";
        QMessageBox::warning(this, "Incomplete Setup", message);
        return;
    }
    
    // Save configuration
    g_installConfig.partitioningMode = PartitioningMode::Manual;
    g_installConfig.bootPartition = bootPartition;
    g_installConfig.rootPartition = rootPartition;
    g_installConfig.swapPartition = swapPartition;
    g_installConfig.bootFormat = bootFormatCombo->currentText().toLower();
    g_installConfig.filesystem = rootFormatCombo->currentText().toLower();
    g_installConfig.enableSwap = !swapPartition.isEmpty();
    
    UserConfigWindow *userConfigWindow = new UserConfigWindow();
    userConfigWindow->setPreviousWindow(this);
    userConfigWindow->setWindowTitle("Arch7z Installer");
    userConfigWindow->resize(1024, 800);
    userConfigWindow->setAttribute(Qt::WA_DeleteOnClose);
    userConfigWindow->show();
    this->hide();
}

void AdvancedPartitionWindow::onBack() {
    parentWidget()->show();
    close();
}