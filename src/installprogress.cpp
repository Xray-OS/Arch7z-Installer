#include "installprogress.h"
#include "vminstaller.h"
#include "vmdetection.h"
#include <QApplication>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QProcess>

InstallProgressWindow::InstallProgressWindow(const InstallConfig &config, QWidget *parent)
    : QMainWindow(parent), config(config) {
    setupUI();
    
    // Use VM installer if virtual machine is detected
    if (VMDetection::isVirtualMachine()) {
        installer = new VMInstaller(config, this);
    } else {
        installer = new Installer(config, this);
    }
    
    connect(installer, &Installer::progressChanged, this, &InstallProgressWindow::onProgressChanged);
    connect(installer, &Installer::installationFinished, this, &InstallProgressWindow::onInstallationFinished);
    
    // Start installation automatically
    installer->startInstallation();
}

void InstallProgressWindow::setupUI() {
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
    
    QLabel *subtitle = new QLabel("Installing System", this);
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setStyleSheet("font-size: 14px; color: #888; margin-bottom: 30px;");
    
    mainLayout->addWidget(title);
    mainLayout->addWidget(logo);
    mainLayout->addWidget(subtitle);
    
    // Progress bar
    progressBar = new QProgressBar(this);
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    progressBar->setStyleSheet(
        "QProgressBar {"
        "  border: 2px solid #18e8ec;"
        "  border-radius: 8px;"
        "  text-align: center;"
        "  font-size: 14px;"
        "  font-weight: bold;"
        "  color: white;"
        "  background-color: #1a1a2e;"
        "}"
        "QProgressBar::chunk {"
        "  background-color: #18e8ec;"
        "  border-radius: 6px;"
        "}"
    );
    
    // Status label
    statusLabel = new QLabel("Preparing installation...", this);
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLabel->setStyleSheet("font-size: 14px; color: white; margin: 20px;");
    
    mainLayout->addWidget(progressBar);
    mainLayout->addWidget(statusLabel);
    mainLayout->addStretch();
    
    // Buttons (initially hidden)
    rebootButton = new QPushButton("Reboot", this);
    rebootButton->setVisible(false);
    rebootButton->setStyleSheet(
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
    connect(rebootButton, &QPushButton::clicked, this, &InstallProgressWindow::onReboot);
    
    quitButton = new QPushButton("Quit", this);
    quitButton->setVisible(false);
    quitButton->setStyleSheet(
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
    connect(quitButton, &QPushButton::clicked, this, &InstallProgressWindow::onQuit);
    
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(quitButton);
    buttonLayout->addWidget(rebootButton);
    buttonLayout->addStretch();
    mainLayout->addLayout(buttonLayout);
    
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

void InstallProgressWindow::onProgressChanged(int percentage, const QString &message) {
    progressBar->setValue(percentage);
    statusLabel->setText(message);
}

void InstallProgressWindow::onInstallationFinished(bool success, const QString &message) {
    if (success) {
        progressBar->setValue(100);
        statusLabel->setText("Installation completed successfully!");
        rebootButton->setVisible(true);
        quitButton->setVisible(true);
    } else {
        statusLabel->setText("Installation failed: " + message);
        quitButton->setVisible(true);
        QMessageBox::critical(this, "Installation Failed", message);
    }
}

void InstallProgressWindow::onReboot() {
    QProcess::startDetached("reboot", QStringList());
    QApplication::quit();
}

void InstallProgressWindow::onQuit() {
    QApplication::quit();
}