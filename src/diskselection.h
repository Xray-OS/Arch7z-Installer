#pragma once
#include <QMainWindow>
#include <QListWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>

struct DiskInfo {
    QString device;
    QString size;
    QString model;
    QString type;
};

class DiskSelectionWindow : public QMainWindow {
    Q_OBJECT

public:
    DiskSelectionWindow(QWidget *parent = nullptr);
    QList<DiskInfo> getAvailableDisks();

private slots:
    void onContinue();
    void onBack();
    void onDiskSelectionChanged();

private:
    void setupUI();
    void loadDisks();
    
    QWidget *centralWidget;
    QVBoxLayout *mainLayout;
    QListWidget *diskList;
    QPushButton *continueButton;
    QPushButton *backButton;
    QList<DiskInfo> disks;
};