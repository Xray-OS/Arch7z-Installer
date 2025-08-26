#pragma once
#include <QMainWindow>
#include <QComboBox>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMap>
#include "locale.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);

private slots:
    void onRegionChanged();
    void onKeyboardLayoutChanged();
    void onKeyboardVariantChanged();
    void onContinue();

private:
    void setupUI();
    void loadData();
    QString getLanguageCode(const QString &languageName);
    
    QWidget *centralWidget;
    QVBoxLayout *mainLayout;
    
    QComboBox *languageCombo;
    QComboBox *regionCombo;
    QComboBox *timezoneCombo;
    QComboBox *keyboardLayoutCombo;
    QComboBox *keyboardVariantCombo;
    QLineEdit *testInput;
    QPushButton *continueButton;
    
    QMap<QString, QStringList> timezoneData;
    QList<KeyboardLayout> keyboardLayouts;
};