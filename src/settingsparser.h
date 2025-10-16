#pragma once
#include <QString>
#include <QStringList>
#include <QMap>

struct CommandSection {
    bool enabled = true;
    int timeout = 120;
    bool requiresInternet = false;
    QString condition;
    QStringList commands;
};

class SettingsParser {
public:
    static bool loadSettings(const QString &configPath = "/usr/share/arch7z-installer/settings/final-settings.conf");
    static QStringList getExecutionCommands();
    static bool hasInternetRequiredCommands();
    static QString getBootloaderId();
    
private:
    static QMap<QString, CommandSection> sections;
    static QMap<QString, QString> variables;
    static bool checkCondition(const QString &condition);
    static QString expandVariables(const QString &command);
};