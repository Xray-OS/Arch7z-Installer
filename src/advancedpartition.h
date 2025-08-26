#pragma once
#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QComboBox>
#include <QGroupBox>
#include <QProcess>

struct PartitionInfo {
    QString device;
    QString label;
    QString size;
    QString filesystem;
    QString mountpoint;
    bool isAssigned;
    QString assignedAs; // "boot", "root", "swap"
};

class AdvancedPartitionWindow : public QMainWindow {
    Q_OBJECT

public:
    AdvancedPartitionWindow(QWidget *parent = nullptr);

private slots:
    void onRefresh();
    void onOpenGParted();
    void onDriveChanged();

    void onPartitionSelected();
    void onSetBootPartition();
    void onSetRootPartition();
    void onSetSwapPartition();
    void onClearPartition();
    void onContinue();
    void onBack();

private:
    void setupUI();
    void refreshPartitions();
    void updatePartitionTable();
    void populateDriveCombo();
    void updateContinueButton();
    bool validatePartitions();
    
    QWidget *centralWidget;
    QVBoxLayout *mainLayout;
    QTableWidget *partitionTable;
    QGroupBox *assignmentGroup;
    QComboBox *bootFormatCombo;
    QComboBox *rootFormatCombo;
    QPushButton *refreshButton;
    QPushButton *gpartedButton;

    QPushButton *setBootButton;
    QPushButton *setRootButton;
    QPushButton *setSwapButton;
    QPushButton *clearButton;
    QPushButton *continueButton;
    QPushButton *backButton;
    
    QList<PartitionInfo> partitions;
    QMap<QString, QList<PartitionInfo>> drivePartitions;
    QComboBox *driveCombo;
    QString currentDrive;
    QString bootPartition;
    QString rootPartition;
    QString swapPartition;
    int selectedRow;
};