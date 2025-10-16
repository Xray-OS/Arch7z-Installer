#include "settingsparser.h"
#include "vmdetection.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>

QMap<QString, CommandSection> SettingsParser::sections;
QMap<QString, QString> SettingsParser::variables;

bool SettingsParser::loadSettings(const QString &configPath) {
    qDebug() << "[SettingsParser] Loading settings from:" << configPath;
    
    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "[SettingsParser] Failed to open settings file:" << configPath;
        return false;
    }
    
    qDebug() << "[SettingsParser] File opened successfully, size:" << file.size() << "bytes";
    
    QTextStream in(&file);
    QString currentSection;
    QString multiLineValue;
    QString multiLineKey;
    
    while (!in.atEnd()) {
        QString line = in.readLine();
        QString trimmedLine = line.trimmed();
        
        if (trimmedLine.isEmpty() || trimmedLine.startsWith('#')) continue;
        
        if (trimmedLine.startsWith('[') && trimmedLine.endsWith(']')) {
            // Process any pending multi-line value
            if (!multiLineKey.isEmpty() && !multiLineValue.isEmpty()) {
                if (multiLineKey == "commands") {
                    QStringList commands = multiLineValue.split(',');
                    for (QString &cmd : commands) {
                        cmd = cmd.trimmed();
                    }
                    commands.removeAll("");
                    sections[currentSection].commands = commands;
                    qDebug() << "[SettingsParser] Added" << commands.size() << "commands to section" << currentSection;
                }
                multiLineKey.clear();
                multiLineValue.clear();
            }
            
            currentSection = trimmedLine.mid(1, trimmedLine.length() - 2);
            qDebug() << "[SettingsParser] Found section:" << currentSection;
            continue;
        }
        
        // Handle multi-line continuation
        if (!multiLineKey.isEmpty()) {
            if (trimmedLine.endsWith(',') || (!trimmedLine.contains('=') && !trimmedLine.isEmpty())) {
                multiLineValue += trimmedLine;
                continue;
            } else {
                // End of multi-line, process it
                multiLineValue += trimmedLine;
                if (multiLineKey == "commands") {
                    QStringList commands = multiLineValue.split(',');
                    for (QString &cmd : commands) {
                        cmd = cmd.trimmed();
                    }
                    commands.removeAll("");
                    sections[currentSection].commands = commands;
                }
                multiLineKey.clear();
                multiLineValue.clear();
                continue;
            }
        }
        
        if (currentSection == "variables") {
            int pos = trimmedLine.indexOf('=');
            if (pos > 0) {
                variables[trimmedLine.left(pos)] = trimmedLine.mid(pos + 1);
            }
        } else if (!currentSection.isEmpty()) {
            int pos = trimmedLine.indexOf('=');
            if (pos > 0) {
                QString key = trimmedLine.left(pos);
                QString value = trimmedLine.mid(pos + 1);
                
                if (key == "commands") {
                    if (value.isEmpty()) {
                        // Start multi-line
                        multiLineKey = key;
                        multiLineValue = "";
                    } else {
                        QStringList commands = value.split(',');
                        for (QString &cmd : commands) {
                            cmd = cmd.trimmed();
                        }
                        commands.removeAll("");
                        sections[currentSection].commands = commands;
                    }
                } else if (key == "enabled") {
                    sections[currentSection].enabled = (value == "true");
                } else if (key == "requires_internet") {
                    sections[currentSection].requiresInternet = (value == "true");
                } else if (key == "timeout") {
                    sections[currentSection].timeout = value.toInt();
                } else if (key == "condition") {
                    sections[currentSection].condition = value;
                }
            }
        }
    }
    
    // Process any final multi-line value
    if (!multiLineKey.isEmpty() && !multiLineValue.isEmpty()) {
        if (multiLineKey == "commands") {
            QStringList commands = multiLineValue.split(',');
            for (QString &cmd : commands) {
                cmd = cmd.trimmed();
            }
            commands.removeAll("");
            sections[currentSection].commands = commands;
            qDebug() << "[SettingsParser] Added final" << commands.size() << "commands to section" << currentSection;
        }
    }
    
    qDebug() << "[SettingsParser] Finished loading. Total sections:" << sections.size();
    for (auto it = sections.begin(); it != sections.end(); ++it) {
        qDebug() << "[SettingsParser] Section" << it.key() << "- enabled:" << it.value().enabled << "commands:" << it.value().commands.size();
    }
    
    return true;
}

QStringList SettingsParser::getExecutionCommands() {
    qDebug() << "[SettingsParser] Getting execution commands...";
    QStringList commands;
    
    // Execute sections in specific order
    QStringList sectionOrder = {"system_commands", "bootloader_commands", "vm_bootloader_commands", 
                                "external_scripts", "cleanup_commands", "network_commands"};
    
    qDebug() << "[SettingsParser] Processing sections in order:" << sectionOrder;
    
    for (const QString &sectionName : sectionOrder) {
        if (!sections.contains(sectionName)) {
            qDebug() << "[SettingsParser] Section" << sectionName << "not found, skipping";
            continue;
        }
        
        const CommandSection &section = sections[sectionName];
        qDebug() << "[SettingsParser] Processing section" << sectionName << "- enabled:" << section.enabled << "commands:" << section.commands.size();
        
        if (!section.enabled) {
            qDebug() << "[SettingsParser] Section" << sectionName << "is disabled, skipping";
            continue;
        }
        if (!section.condition.isEmpty() && !checkCondition(section.condition)) {
            qDebug() << "[SettingsParser] Section" << sectionName << "condition not met:" << section.condition;
            continue;
        }
        
        for (const QString &cmd : section.commands) {
            QString expandedCmd = expandVariables(cmd.trimmed());
            QString finalCmd;
            if (sectionName == "external_scripts") {
                finalCmd = QString("arch-chroot /mnt /usr/local/bin/%1").arg(expandedCmd);
            } else {
                finalCmd = QString("arch-chroot /mnt %1").arg(expandedCmd);
            }
            commands << finalCmd;
            qDebug() << "[SettingsParser] Added command:" << finalCmd;
        }
    }
    
    qDebug() << "[SettingsParser] Total commands generated:" << commands.size();
    return commands;
}

bool SettingsParser::checkCondition(const QString &condition) {
    if (condition == "vm_detected") {
        return VMDetection::isVirtualMachine();
    }
    return true;
}

QString SettingsParser::expandVariables(const QString &command) {
    QString result = command;
    for (auto it = variables.begin(); it != variables.end(); ++it) {
        result.replace("${" + it.key() + "}", it.value());
    }
    return result;
}

QString SettingsParser::getBootloaderId() {
    return variables.value("bootloader_id", "Xray_OS");
}