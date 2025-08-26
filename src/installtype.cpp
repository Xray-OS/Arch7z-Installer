#include "installtype.h"
#include "diskselection.h"
#include "advancedpartition.h"
#include "vmpartitionlayout.h"
#include "installconfig.h"
#include "vmdetection.h"
#include <QApplication>
#include <QGridLayout>
#include <QMessageBox>
#include <QButtonGroup>

extern InstallConfig g_installConfig;

InstallTypeWindow::InstallTypeWindow(QWidget *parent) : QMainWindow(parent) {
    setupUI();
}

void InstallTypeWindow::setupUI() {
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
    
    QLabel *subtitle = new QLabel("Choose Installation Type", this);
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setStyleSheet("font-size: 14px; color: #888; margin-bottom: 30px;");
    
    mainLayout->addWidget(title);
    mainLayout->addWidget(logo);
    mainLayout->addWidget(subtitle);
    
    // Installation options
    cleanInstallRadio = new QRadioButton("Clean Install", this);
    cleanInstallRadio->setChecked(true);
    cleanInstallRadio->setStyleSheet("font-size: 16px; color: white; margin: 10px;");
    
    QLabel *cleanDesc = new QLabel("Erase the entire selected disk and install Arch7z", this);
    cleanDesc->setStyleSheet("font-size: 12px; color: #888; margin-left: 25px; margin-bottom: 20px;");
    
    customInstallRadio = new QRadioButton("Custom Install", this);
    customInstallRadio->setStyleSheet("font-size: 16px; color: white; margin: 10px;");
    
    QLabel *customDesc = new QLabel("Manually partition the disk and choose installation options", this);
    customDesc->setStyleSheet("font-size: 12px; color: #888; margin-left: 25px; margin-bottom: 20px;");
    
    mainLayout->addWidget(cleanInstallRadio);
    mainLayout->addWidget(cleanDesc);
    mainLayout->addWidget(customInstallRadio);
    mainLayout->addWidget(customDesc);
    mainLayout->addStretch();
    
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
    connect(continueButton, &QPushButton::clicked, this, &InstallTypeWindow::onContinue);
    connect(backButton, &QPushButton::clicked, this, &InstallTypeWindow::onBack);
    
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

void InstallTypeWindow::onContinue() {
    if (cleanInstallRadio->isChecked()) {
        g_installConfig.installationSource = InstallationSource::AutomaticInstall;
        g_installConfig.partitioningMode = PartitioningMode::Automatic;
        
        // Check if VM and route accordingly
        bool isVM = false;
        try {
            isVM = VMDetection::isVirtualMachine();
        } catch (...) {
            isVM = false;
        }
        
        if (isVM) {
            // VM: Skip disk selection, use first available disk
            DiskSelectionWindow diskWindow;
            auto disks = diskWindow.getAvailableDisks();
            if (!disks.isEmpty()) {
                g_installConfig.selectedDisk = disks.first().device;
                g_installConfig.diskSize = disks.first().size;
                
                VMPartitionLayoutWindow *vmWindow = new VMPartitionLayoutWindow(disks.first().device, disks.first().size, this);
                vmWindow->setWindowTitle("Arch7z Installer");
                vmWindow->resize(1024, 800);
                vmWindow->show();
                this->hide();
            }
        } else {
            // Physical: Normal disk selection flow
            DiskSelectionWindow *diskWindow = new DiskSelectionWindow(this);
            diskWindow->setWindowTitle("Arch7z Installer");
            diskWindow->resize(1024, 800);
            diskWindow->show();
            this->hide();
        }
    } else {
        g_installConfig.installationSource = InstallationSource::CustomPartitioning;
        g_installConfig.partitioningMode = PartitioningMode::Manual;
        
        AdvancedPartitionWindow *advancedWindow = new AdvancedPartitionWindow(this);
        advancedWindow->setWindowTitle("Arch7z Installer");
        advancedWindow->resize(1200, 800);
        advancedWindow->show();
        this->hide();
    }
}

void InstallTypeWindow::onBack() {
    // Return to main window
    parentWidget()->show();
    close();
}