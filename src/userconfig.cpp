#include "userconfig.h"
#include "installconfig.h"
#include "installprogress.h"
#include <QApplication>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QDebug>

extern InstallConfig g_installConfig;

UserConfigWindow::UserConfigWindow(QWidget *parent) : QMainWindow(parent), previousWindow(nullptr) {
    setupUI();
}

void UserConfigWindow::setPreviousWindow(QWidget *window) {
    previousWindow = window;
}

void UserConfigWindow::setupUI() {
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
    
    QLabel *subtitle = new QLabel("User Configuration", this);
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setStyleSheet("font-size: 14px; color: #888; margin-bottom: 30px;");
    
    mainLayout->addWidget(title);
    mainLayout->addWidget(logo);
    mainLayout->addWidget(subtitle);
    
    // Form layout
    formLayout = new QFormLayout();
    formLayout->setSpacing(15);
    
    // Hostname
    hostnameEdit = new QLineEdit("Xray_OS", this);
    hostnameEdit->setStyleSheet(
        "QLineEdit {"
        "  background-color: #16213e;"
        "  border: 1px solid #18e8ec;"
        "  border-radius: 6px;"
        "  padding: 8px;"
        "  color: white;"
        "  font-size: 14px;"
        "}"
    );
    formLayout->addRow("Hostname:", hostnameEdit);
    
    // Username
    usernameEdit = new QLineEdit(this);
    usernameEdit->setStyleSheet(
        "QLineEdit {"
        "  background-color: #16213e;"
        "  border: 1px solid #18e8ec;"
        "  border-radius: 6px;"
        "  padding: 8px;"
        "  color: white;"
        "  font-size: 14px;"
        "}"
    );
    formLayout->addRow("Username:", usernameEdit);
    
    // Password
    passwordEdit = new QLineEdit(this);
    passwordEdit->setEchoMode(QLineEdit::Password);
    passwordEdit->setStyleSheet(
        "QLineEdit {"
        "  background-color: #16213e;"
        "  border: 1px solid #18e8ec;"
        "  border-radius: 6px;"
        "  padding: 8px;"
        "  color: white;"
        "  font-size: 14px;"
        "}"
    );
    formLayout->addRow("Password:", passwordEdit);
    
    // Root Password
    rootPasswordEdit = new QLineEdit(this);
    rootPasswordEdit->setEchoMode(QLineEdit::Password);
    rootPasswordEdit->setStyleSheet(
        "QLineEdit {"
        "  background-color: #16213e;"
        "  border: 1px solid #18e8ec;"
        "  border-radius: 6px;"
        "  padding: 8px;"
        "  color: white;"
        "  font-size: 14px;"
        "}"
    );
    formLayout->addRow("Root Password:", rootPasswordEdit);
    
    // Same password checkbox
    samePasswordCheckBox = new QCheckBox("Use same password for root", this);
    samePasswordCheckBox->setChecked(true);
    samePasswordCheckBox->setStyleSheet("font-size: 16px; color: white; padding: 8px;");
    connect(samePasswordCheckBox, &QCheckBox::toggled, this, &UserConfigWindow::onSamePasswordToggled);
    formLayout->addRow("", samePasswordCheckBox);
    
    // Shell selection
    shellCombo = new QComboBox(this);
    shellCombo->addItems(QStringList() << "fish" << "bash" << "zsh" << "sh");
    shellCombo->setCurrentText("fish");
    shellCombo->setStyleSheet(
        "QComboBox {"
        "  background-color: #16213e;"
        "  border: 1px solid #18e8ec;"
        "  border-radius: 6px;"
        "  padding: 8px;"
        "  color: white;"
        "  font-size: 14px;"
        "}"
    );
    formLayout->addRow("Shell:", shellCombo);
    
    // Set form label styling
    for (int i = 0; i < formLayout->rowCount(); ++i) {
        QLayoutItem *item = formLayout->itemAt(i, QFormLayout::LabelRole);
        if (item && item->widget()) {
            QLabel *label = qobject_cast<QLabel*>(item->widget());
            if (label) {
                label->setStyleSheet("color: white; font-size: 14px; font-weight: bold;");
            }
        }
    }
    
    mainLayout->addLayout(formLayout);
    mainLayout->addStretch();
    
    // Initially disable root password field
    rootPasswordEdit->setEnabled(false);
    
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
    connect(continueButton, &QPushButton::clicked, this, &UserConfigWindow::onContinue);
    connect(backButton, &QPushButton::clicked, this, &UserConfigWindow::onBack);
    
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

void UserConfigWindow::onSamePasswordToggled() {
    rootPasswordEdit->setEnabled(!samePasswordCheckBox->isChecked());
    if (samePasswordCheckBox->isChecked()) {
        rootPasswordEdit->clear();
    }
}

bool UserConfigWindow::validateInputs() {
    if (hostnameEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "Hostname cannot be empty.");
        return false;
    }
    
    if (usernameEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "Username cannot be empty.");
        return false;
    }
    
    if (passwordEdit->text().isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "Password cannot be empty.");
        return false;
    }
    
    if (!samePasswordCheckBox->isChecked() && rootPasswordEdit->text().isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "Root password cannot be empty when using separate passwords.");
        return false;
    }
    
    return true;
}

void UserConfigWindow::onContinue() {
    if (!validateInputs()) {
        return;
    }
    
    QString hostname = hostnameEdit->text().trimmed();
    QString username = usernameEdit->text().trimmed();
    QString password = passwordEdit->text();
    QString rootPassword = samePasswordCheckBox->isChecked() ? password : rootPasswordEdit->text();
    QString shell = shellCombo->currentText();
    
    QString message = QString("User Configuration:\n\n"
                             "Hostname: %1\n"
                             "Username: %2\n"
                             "Shell: %3\n"
                             "Same password for root: %4\n\n"
                             "Ready to proceed with installation?")
                             .arg(hostname)
                             .arg(username)
                             .arg(shell)
                             .arg(samePasswordCheckBox->isChecked() ? "Yes" : "No");
    
    int ret = QMessageBox::information(this, "User Configuration Complete", message,
                                      QMessageBox::Ok | QMessageBox::Cancel,
                                      QMessageBox::Ok);
    
    if (ret == QMessageBox::Ok) {
        // Save user settings to global config
        g_installConfig.hostname = hostname;
        g_installConfig.username = username;
        g_installConfig.password = password;
        g_installConfig.rootPassword = rootPassword;
        g_installConfig.samePassword = samePasswordCheckBox->isChecked();
        g_installConfig.shell = shell;
        
        // Start installation
        InstallProgressWindow *progressWindow = new InstallProgressWindow(g_installConfig);
        progressWindow->setWindowTitle("Arch7z Installer");
        progressWindow->resize(1024, 800);
        progressWindow->setAttribute(Qt::WA_DeleteOnClose);
        progressWindow->show();
        this->close();
    }
}

void UserConfigWindow::onBack() {
    hide();
    if (previousWindow) {
        previousWindow->show();
    }
}