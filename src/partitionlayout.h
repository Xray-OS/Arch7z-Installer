#pragma once
#include <QMainWindow>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QTableWidget>
#include <QRadioButton>
#include <QButtonGroup>

class PartitionLayoutWindow : public QMainWindow {
    Q_OBJECT

public:
    PartitionLayoutWindow(const QString &selectedDisk, const QString &diskSize, QWidget *parent = nullptr);

private slots:
    void onContinue();
    void onBack();
    void onSwapToggled();
    void onFilesystemChanged();

private:
    void setupUI();
    void updatePartitionTable();
    QString calculateRemainingSpace();
    
    QWidget *centralWidget;
    QVBoxLayout *mainLayout;
    QTableWidget *partitionTable;
    QCheckBox *swapCheckBox;
    QRadioButton *btrfsRadio;
    QRadioButton *ext4Radio;
    QButtonGroup *filesystemGroup;
    QPushButton *continueButton;
    QPushButton *backButton;
    
    QString selectedDisk;
    QString diskSize;
};