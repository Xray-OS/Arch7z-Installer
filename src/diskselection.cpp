#include "diskselection.h"
#include "partitionlayout.h"
#include "installconfig.h"
#include <QApplication>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QProcess>
#include <QDebug>
#include <QRegularExpression>

extern InstallConfig g_installConfig;

DiskSelectionWindow::DiskSelectionWindow(QWidget *parent) : QMainWindow(parent) {
    setupUI();
    loadDisks();
}

void DiskSelectionWindow::setupUI() {
    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(40, 40, 40, 40);
    
    // Title
    QLabel *title = new QLabel("Arch7z Installer", this);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-size: 28px; font-weight: 300; color: #18e8ec; margin-bottom: 10px;");
    
    // Logo
    QLabel *logo = new QLabel(this);
    QPixmap logoPixmap(":/src/resources/icons/xray-installer.png");
    if (!logoPixmap.isNull()) {
        logo->setPixmap(logoPixmap.scaled(96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        logo->setAlignment(Qt::AlignCenter);
    }
    
    QLabel *subtitle = new QLabel("Select Disk for Clean Install", this);
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setStyleSheet("font-size: 14px; color: #888; margin-bottom: 30px;");
    
    QLabel *warning = new QLabel("⚠️ WARNING: All data on the selected disk will be erased!", this);
    warning->setAlignment(Qt::AlignCenter);
    warning->setStyleSheet("font-size: 14px; color: #ff6b6b; font-weight: bold; margin-bottom: 20px;");
    
    mainLayout->addWidget(title);
    mainLayout->addWidget(logo);
    mainLayout->addWidget(subtitle);
    mainLayout->addWidget(warning);
    
    // Disk list
    diskList = new QListWidget(this);
    diskList->setStyleSheet(
        "QListWidget {"
        "  background-color: #1a1a2e;"
        "  border: 1px solid #18e8ec;"
        "  border-radius: 6px;"
        "  padding: 10px;"
        "  color: white;"
        "  font-size: 14px;"
        "}"
        "QListWidget::item {"
        "  padding: 10px;"
        "  border-bottom: 1px solid #333;"
        "}"
        "QListWidget::item:selected {"
        "  background-color: #18e8ec;"
        "  color: black;"
        "}"
    );
    
    mainLayout->addWidget(diskList);
    
    // Buttons
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
    continueButton->setEnabled(false);
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
    connect(continueButton, &QPushButton::clicked, this, &DiskSelectionWindow::onContinue);
    connect(backButton, &QPushButton::clicked, this, &DiskSelectionWindow::onBack);
    connect(diskList, &QListWidget::itemSelectionChanged, this, &DiskSelectionWindow::onDiskSelectionChanged);
    
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
}

void DiskSelectionWindow::loadDisks() {
    try {
        disks = getAvailableDisks();
        
        if (disks.isEmpty()) {
            diskList->addItem("No disks found. Please check system configuration.");
            return;
        }
        
        for (const DiskInfo &disk : disks) {
            QString displayText = QString("%1 - %2 (%3) - %4")
                                 .arg(disk.device)
                                 .arg(disk.model)
                                 .arg(disk.size)
                                 .arg(disk.type);
            diskList->addItem(displayText);
        }
    } catch (...) {
        diskList->addItem("Error loading disks. Please restart the installer.");
    }
}

QList<DiskInfo> DiskSelectionWindow::getAvailableDisks() {
    QList<DiskInfo> diskList;
    
    QProcess lsblk;
    lsblk.start("lsblk", QStringList() << "-d" << "-n" << "-o" << "NAME,SIZE,MODEL,TYPE,TRAN");
    
    if (!lsblk.waitForFinished(10000)) {
        qDebug() << "lsblk command timed out";
        return diskList;
    }
    
    if (lsblk.exitCode() != 0) {
        qDebug() << "lsblk failed with exit code:" << lsblk.exitCode();
        qDebug() << "Error:" << lsblk.readAllStandardError();
        return diskList;
    }
    
    QString output = lsblk.readAllStandardOutput();
    if (output.isEmpty()) {
        qDebug() << "lsblk returned empty output";
        return diskList;
    }
    
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    
    for (const QString &line : lines) {
        QStringList parts = line.trimmed().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (parts.size() >= 2) {
            QString name = parts[0];
            QString size = parts[1];
            QString model = parts.size() > 2 ? parts[2] : "Unknown";
            QString type = parts.size() > 3 ? parts[3] : "disk";
            QString transport = parts.size() > 4 ? parts[4] : "";
            
            // Skip loop devices and other virtual devices
            if (!name.startsWith("loop") && !name.startsWith("ram") && 
                !name.startsWith("zram") && size != "0B" && !name.isEmpty()) {
                DiskInfo disk;
                disk.device = "/dev/" + name;
                disk.size = size;
                disk.model = model;
                disk.type = transport.isEmpty() ? type : transport;
                diskList.append(disk);
            }
        }
    }
    
    return diskList;
}

void DiskSelectionWindow::onDiskSelectionChanged() {
    continueButton->setEnabled(diskList->currentRow() >= 0);
}

void DiskSelectionWindow::onContinue() {
    int selectedIndex = diskList->currentRow();
    if (selectedIndex >= 0 && selectedIndex < disks.size()) {
        DiskInfo selectedDisk = disks[selectedIndex];
        
        QString message = QString("Selected disk: %1\n"
                                 "Model: %2\n"
                                 "Size: %3\n\n"
                                 "⚠️ ALL DATA WILL BE ERASED!\n\n"
                                 "Are you sure you want to continue?")
                                 .arg(selectedDisk.device)
                                 .arg(selectedDisk.model)
                                 .arg(selectedDisk.size);
        
        int ret = QMessageBox::warning(this, "Confirm Clean Install", message,
                                      QMessageBox::Yes | QMessageBox::No,
                                      QMessageBox::No);
        
        if (ret == QMessageBox::Yes) {
            // Save disk settings to global config
            g_installConfig.selectedDisk = selectedDisk.device;
            g_installConfig.diskSize = selectedDisk.size;
            
            PartitionLayoutWindow *partitionWindow = new PartitionLayoutWindow(selectedDisk.device, selectedDisk.size, this);
            partitionWindow->setWindowTitle("Arch7z Installer");
            partitionWindow->resize(1024, 800);
            partitionWindow->show();
            this->hide();
        }
    }
}

void DiskSelectionWindow::onBack() {
    // Return to install type window
    parentWidget()->show();
    close();
}