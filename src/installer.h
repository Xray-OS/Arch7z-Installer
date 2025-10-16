#pragma once
#include <QObject>
#include <QProcess>
#include <QTimer>
#include "installconfig.h"
#include "settingsparser.h"

class Installer : public QObject {
    Q_OBJECT

public:
    explicit Installer(const InstallConfig &config, QObject *parent = nullptr);
    void startInstallation();

signals:
    void progressChanged(int percentage, const QString &message);
    void installationFinished(bool success, const QString &message);

private slots:
    void executeNextStep();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus = QProcess::NormalExit);

protected:
    virtual void partitionDisk();
    virtual void formatPartitions();
    virtual void mountPartitions();
    virtual void installBaseSystem();
    virtual void configureSystem();
    virtual void installBootloader();
    virtual void runCustomScripts();

    virtual void cleanup();

private:
    
    void updateProgress(int percentage, const QString &message);
    void writeFile(const QString &path, const QString &content);
    void appendFile(const QString &path, const QString &content);
    QString getPartitionName(const QString &disk, int partitionNumber);
    void terminateInstallation();
    
protected:
    InstallConfig config;
    void executeCommand(const QString &command, const QStringList &args = QStringList());
    
private:
    QProcess *currentProcess;
    QTimer *progressTimer;
    
    int currentStep;
    int totalSteps;
    QStringList installSteps;
};