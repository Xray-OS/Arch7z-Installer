#include <QCoreApplication>
#include <QDebug>
#include "src/installconfig.h"

// Simulate the getLanguageCode function
QString getLanguageCode(const QString &languageName) {
    static QMap<QString, QString> languageMap = {
        {"English", "en_US"},
        {"Español (Spanish)", "es_ES"},
        {"Français (French)", "fr_FR"},
        {"Deutsch (German)", "de_DE"}
    };
    return languageMap.value(languageName, "en_US");
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    
    // Test the locale formatting
    InstallConfig config;
    
    // Simulate what MainWindow::onContinue() does
    QString languageCode = getLanguageCode("English");
    config.language = languageCode + ".UTF-8 UTF-8";
    config.region = "America";
    config.timezone = "America/Caracas";
    config.keyboardLayout = "us";
    config.keyboardVariant = "alt-intl";
    
    qDebug() << "=== Locale Configuration Test ===";
    qDebug() << "Language (for locale.gen):" << config.language;
    qDebug() << "Timezone (full path):" << config.timezone;
    qDebug() << "Keyboard Layout:" << config.keyboardLayout;
    qDebug() << "Keyboard Variant:" << config.keyboardVariant;
    
    // Test what installer.cpp will extract
    QString langCode = config.language.split(" ").first();
    qDebug() << "LANG code (for locale.conf):" << langCode;
    
    qDebug() << "\n=== Expected Jade Format ===";
    qDebug() << "locale.gen should contain: en_US.UTF-8 UTF-8";
    qDebug() << "locale.conf should contain: LANG=en_US.UTF-8";
    qDebug() << "timezone path should be: /usr/share/zoneinfo/America/Caracas";
    
    return 0;
}