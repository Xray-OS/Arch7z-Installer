#include "vmpartitionlayout.h"
#include "userconfig.h"
#include "installconfig.h"
#include <QApplication>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QHeaderView>

extern InstallConfig g_installConfig;

VMPartitionLayoutWindow::VMPartitionLayoutWindow(const QString &selectedDisk, const QString &diskSize, QWidget *parent) 
    : QMainWindow(parent), selectedDisk(selectedDisk), diskSize(diskSize) {
    setupUI();
    updatePartitionTable();
}

void VMPartitionLayoutWindow::setupUI() {
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
    
    QLabel *subtitle = new QLabel("VM Partition Layout", this);
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setStyleSheet("font-size: 14px; color: #888; margin-bottom: 20px;");
    
    QLabel *diskInfo = new QLabel(QString("Selected Disk: %1 (%2)").arg(selectedDisk).arg(diskSize), this);
    diskInfo->setAlignment(Qt::AlignCenter);
    diskInfo->setStyleSheet("font-size: 14px; color: #18e8ec; margin-bottom: 30px;");
    
    mainLayout->addWidget(title);
    mainLayout->addWidget(logo);
    mainLayout->addWidget(subtitle);
    mainLayout->addWidget(diskInfo);
    
    // Filesystem selection
    QLabel *fsLabel = new QLabel("Root Filesystem:", this);
    fsLabel->setStyleSheet("font-size: 16px; color: white; margin-bottom: 10px;");
    mainLayout->addWidget(fsLabel);
    
    QHBoxLayout *fsLayout = new QHBoxLayout();
    filesystemGroup = new QButtonGroup(this);
    
    btrfsRadio = new QRadioButton("Btrfs (recommended)", this);
    btrfsRadio->setChecked(true);
    btrfsRadio->setStyleSheet("font-size: 14px; color: white; margin-right: 20px;");
    
    ext4Radio = new QRadioButton("Ext4", this);
    ext4Radio->setStyleSheet("font-size: 14px; color: white;");
    
    filesystemGroup->addButton(btrfsRadio);
    filesystemGroup->addButton(ext4Radio);
    
    fsLayout->addWidget(btrfsRadio);
    fsLayout->addWidget(ext4Radio);
    fsLayout->addStretch();
    mainLayout->addLayout(fsLayout);
    
    connect(btrfsRadio, &QRadioButton::toggled, this, &VMPartitionLayoutWindow::onFilesystemChanged);
    
    // Partition table
    partitionTable = new QTableWidget(this);
    partitionTable->setColumnCount(4);
    partitionTable->setHorizontalHeaderLabels(QStringList() << "Partition" << "Size" << "Type" << "Mount Point");
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
    
    mainLayout->addWidget(partitionTable);
    
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
    );
    
    buttonLayout->addWidget(backButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(continueButton);
    mainLayout->addLayout(buttonLayout);
    
    // Connect signals
    connect(continueButton, &QPushButton::clicked, this, &VMPartitionLayoutWindow::onContinue);
    connect(backButton, &QPushButton::clicked, this, &VMPartitionLayoutWindow::onBack);
    
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

void VMPartitionLayoutWindow::updatePartitionTable() {
    // VM: Single root partition (MBR)
    partitionTable->setRowCount(1);
    QString fsType = btrfsRadio->isChecked() ? "Btrfs" : "Ext4";
    partitionTable->setItem(0, 0, new QTableWidgetItem(selectedDisk + "1"));
    partitionTable->setItem(0, 1, new QTableWidgetItem("Full disk"));
    partitionTable->setItem(0, 2, new QTableWidgetItem(fsType));
    partitionTable->setItem(0, 3, new QTableWidgetItem("/"));
}

void VMPartitionLayoutWindow::onContinue() {
    // Save settings to global config
    g_installConfig.enableSwap = false; // No swap for VMs
    g_installConfig.filesystem = btrfsRadio->isChecked() ? "btrfs" : "ext4";
    
    UserConfigWindow *userConfigWindow = new UserConfigWindow();
    userConfigWindow->setPreviousWindow(this);
    userConfigWindow->setWindowTitle("Arch7z Installer");
    userConfigWindow->resize(1024, 800);
    userConfigWindow->setAttribute(Qt::WA_DeleteOnClose);
    userConfigWindow->show();
    this->hide();
}

void VMPartitionLayoutWindow::onFilesystemChanged() {
    updatePartitionTable();
}

void VMPartitionLayoutWindow::onBack() {
    parentWidget()->show();
    close();
}