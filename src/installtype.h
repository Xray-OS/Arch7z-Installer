#pragma once
#include <QMainWindow>
#include <QRadioButton>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>

class InstallTypeWindow : public QMainWindow {
    Q_OBJECT

public:
    InstallTypeWindow(QWidget *parent = nullptr);

private slots:
    void onContinue();
    void onBack();

private:
    void setupUI();
    
    QWidget *centralWidget;
    QVBoxLayout *mainLayout;
    QRadioButton *cleanInstallRadio;
    QRadioButton *customInstallRadio;
    QPushButton *continueButton;
    QPushButton *backButton;
};