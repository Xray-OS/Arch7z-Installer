#pragma once
#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QRadioButton>
#include <QButtonGroup>

class VMPartitionLayoutWindow : public QMainWindow {
    Q_OBJECT

public:
    VMPartitionLayoutWindow(const QString &selectedDisk, const QString &diskSize, QWidget *parent = nullptr);

private slots:
    void onContinue();
    void onBack();
    void onFilesystemChanged();

private:
    void setupUI();
    void updatePartitionTable();
    
    QWidget *centralWidget;
    QVBoxLayout *mainLayout;
    QTableWidget *partitionTable;
    QButtonGroup *filesystemGroup;
    QRadioButton *btrfsRadio;
    QRadioButton *ext4Radio;
    QPushButton *continueButton;
    QPushButton *backButton;
    
    QString selectedDisk;
    QString diskSize;
};