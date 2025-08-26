#pragma once
#include <QMainWindow>
#include <QVBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include "installconfig.h"
#include "installer.h"

class InstallProgressWindow : public QMainWindow {
    Q_OBJECT

public:
    InstallProgressWindow(const InstallConfig &config, QWidget *parent = nullptr);

private slots:
    void onProgressChanged(int percentage, const QString &message);
    void onInstallationFinished(bool success, const QString &message);
    void onReboot();
    void onQuit();

private:
    void setupUI();
    
    QWidget *centralWidget;
    QVBoxLayout *mainLayout;
    QProgressBar *progressBar;
    QLabel *statusLabel;
    QPushButton *rebootButton;
    QPushButton *quitButton;
    
    Installer *installer;
    InstallConfig config;
};