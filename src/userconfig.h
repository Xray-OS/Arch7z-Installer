#pragma once
#include <QMainWindow>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QLineEdit>
#include <QComboBox>

class UserConfigWindow : public QMainWindow {
    Q_OBJECT

public:
    UserConfigWindow(QWidget *parent = nullptr);
    void setPreviousWindow(QWidget *window);

private slots:
    void onContinue();
    void onBack();
    void onSamePasswordToggled();

private:
    void setupUI();
    bool validateInputs();
    
    QWidget *centralWidget;
    QVBoxLayout *mainLayout;
    QFormLayout *formLayout;
    
    QLineEdit *hostnameEdit;
    QLineEdit *usernameEdit;
    QLineEdit *passwordEdit;
    QLineEdit *rootPasswordEdit;
    QCheckBox *samePasswordCheckBox;
    QComboBox *shellCombo;
    
    QPushButton *continueButton;
    QPushButton *backButton;
    
    QWidget *previousWindow;
};