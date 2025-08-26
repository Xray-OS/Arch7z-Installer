#include "locale.h"
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QSet>

QStringList LocaleData::loadLanguages() {
    QStringList languages;
    QFile file("/usr/share/i18n/SUPPORTED");
    
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        QSet<QString> langSet;
        
        while (!in.atEnd()) {
            QString line = in.readLine();
            QStringList parts = line.split('_');
            if (!parts.isEmpty()) {
                QString langCode = parts[0];
                QString langName = getLanguageName(langCode);
                langSet.insert(langName);
            }
        }
        
        languages = langSet.values();
        languages.sort();
    }
    
    return languages;
}

QStringList LocaleData::loadRegions() {
    return {"Africa", "America", "Antarctica", "Arctic", "Asia", "Atlantic", "Australia", "Europe", "Indian", "Pacific"};
}

QMap<QString, QStringList> LocaleData::loadTimezones() {
    QMap<QString, QStringList> timezones;
    
    QProcess process;
    process.start("timedatectl", QStringList() << "list-timezones");
    process.waitForFinished(5000); // 5 second timeout
    
    if (process.exitCode() == 0) {
        QString output = process.readAllStandardOutput();
        QStringList allTimezones = output.split('\n', Qt::SkipEmptyParts);
        
        for (const QString &tz : allTimezones) {
            QString trimmed = tz.trimmed();
            if (trimmed.contains('/')) {
                QStringList parts = trimmed.split('/');
                if (parts.size() >= 2) {
                    QString region = parts[0];
                    QString zone = parts.mid(1).join('/');
                    
                    timezones[region].append(zone);
                }
            }
        }
        
        // Sort zones in each region
        for (auto it = timezones.begin(); it != timezones.end(); ++it) {
            it.value().sort();
        }
    }
    
    return timezones;
}

QList<KeyboardLayout> LocaleData::loadKeyboardLayouts() {
    QList<KeyboardLayout> layouts;
    
    QStringList priorityLayouts = {
        "us", "gb", "de", "fr", "es", "it", "pt", "ru", "jp", "kr", 
        "cn", "ar", "tr", "pl", "nl", "se", "no", "dk", "fi", "cz"
    };
    
    QProcess process;
    process.start("localectl", QStringList() << "list-x11-keymap-layouts");
    process.waitForFinished();
    
    QStringList allLayouts;
    if (process.exitCode() == 0) {
        QString output = process.readAllStandardOutput();
        allLayouts = output.split('\n', Qt::SkipEmptyParts);
    }
    
    for (const QString &priority : priorityLayouts) {
        if (allLayouts.contains(priority) || allLayouts.isEmpty()) {
            KeyboardLayout layout;
            layout.name = priority;
            layout.description = getLayoutDescription(priority);
            layout.variants = getLayoutVariants(priority);
            layouts.append(layout);
        }
    }
    
    for (const QString &name : allLayouts.mid(0, 50)) {
        QString trimmed = name.trimmed();
        if (!trimmed.isEmpty() && !priorityLayouts.contains(trimmed)) {
            KeyboardLayout layout;
            layout.name = trimmed;
            layout.description = getLayoutDescription(trimmed);
            layout.variants = getLayoutVariants(trimmed);
            layouts.append(layout);
            
            if (layouts.size() >= 30) break;
        }
    }
    
    return layouts;
}

bool LocaleData::setKeyboardLayout(const QString &layout, const QString &variant) {
    QProcess setxkb;
    QStringList setxkbArgs;
    
    // When running as root (sudo), we need to set for the actual display
    QString display = qgetenv("DISPLAY");
    if (!display.isEmpty()) {
        setxkbArgs << "-display" << display;
    }
    
    setxkbArgs << "-layout" << layout;
    if (!variant.isEmpty()) {
        setxkbArgs << "-variant" << variant;
    }
    
    setxkb.start("setxkbmap", setxkbArgs);
    setxkb.waitForFinished();
    
    return setxkb.exitCode() == 0;
}

QString LocaleData::getLanguageName(const QString &code) {
    static QMap<QString, QString> names = {
        {"en", "English"},
        {"es", "Español (Spanish)"},
        {"fr", "Français (French)"},
        {"de", "Deutsch (German)"},
        {"it", "Italiano (Italian)"},
        {"pt", "Português (Portuguese)"},
        {"ru", "Русский (Russian)"},
        {"zh", "中文 (Chinese)"},
        {"ja", "日本語 (Japanese)"},
        {"ko", "한국어 (Korean)"},
        {"ar", "العربية (Arabic)"},
        {"hi", "हिन्दी (Hindi)"},
        {"tr", "Türkçe (Turkish)"},
        {"pl", "Polski (Polish)"},
        {"nl", "Nederlands (Dutch)"},
        {"sv", "Svenska (Swedish)"},
        {"da", "Dansk (Danish)"},
        {"no", "Norsk (Norwegian)"},
        {"fi", "Suomi (Finnish)"},
        {"cs", "Čeština (Czech)"},
        {"hu", "Magyar (Hungarian)"},
        {"ro", "Română (Romanian)"},
        {"bg", "Български (Bulgarian)"},
        {"hr", "Hrvatski (Croatian)"},
        {"sk", "Slovenčina (Slovak)"},
        {"sl", "Slovenščina (Slovenian)"},
        {"et", "Eesti (Estonian)"},
        {"lv", "Latviešu (Latvian)"},
        {"lt", "Lietuvių (Lithuanian)"},
        {"uk", "Українська (Ukrainian)"},
        {"be", "Беларуская (Belarusian)"},
        {"sr", "Српски (Serbian)"},
        {"bs", "Bosanski (Bosnian)"},
        {"mk", "Македонски (Macedonian)"},
        {"sq", "Shqip (Albanian)"},
        {"el", "Ελληνικά (Greek)"},
        {"mt", "Malti (Maltese)"},
        {"is", "Íslenska (Icelandic)"},
        {"ga", "Gaeilge (Irish)"}
    };
    
    return names.value(code, code);
}

QString LocaleData::getLayoutDescription(const QString &code) {
    QFile file("/usr/share/X11/xkb/rules/evdev.lst");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        bool inLayoutSection = false;
        
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            
            if (line == "! layout") {
                inLayoutSection = true;
                continue;
            }
            if (line.startsWith("!") && inLayoutSection) {
                break;
            }
            
            if (inLayoutSection && line.startsWith(code + " ")) {
                QString description = line.mid(code.length() + 1).trimmed();
                return description.isEmpty() ? code.toUpper() : description;
            }
        }
    }
    
    return code.left(1).toUpper() + code.mid(1);
}

QList<KeyboardVariant> LocaleData::getLayoutVariants(const QString &layout) {
    QList<KeyboardVariant> variants;
    
    KeyboardVariant basic;
    basic.name = "";
    basic.description = "Default";
    variants.append(basic);
    
    QProcess process;
    process.start("localectl", QStringList() << "list-x11-keymap-variants" << layout);
    process.waitForFinished();
    
    if (process.exitCode() == 0) {
        QString output = process.readAllStandardOutput();
        QStringList variantNames = output.split('\n', Qt::SkipEmptyParts);
        
        for (const QString &name : variantNames.mid(0, 8)) {
            QString trimmed = name.trimmed();
            if (!trimmed.isEmpty()) {
                KeyboardVariant variant;
                variant.name = trimmed;
                variant.description = getVariantDisplayName(layout, trimmed);
                variants.append(variant);
            }
        }
    }
    
    return variants;
}

QString LocaleData::getVariantDisplayName(const QString &layout, const QString &variant) {
    QFile file("/usr/share/X11/xkb/rules/evdev.lst");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        bool inVariantSection = false;
        
        while (!in.atEnd()) {
            QString line = in.readLine();
            
            if (line.trimmed() == "! variant") {
                inVariantSection = true;
                continue;
            }
            if (line.startsWith("!") && inVariantSection) {
                break;
            }
            
            if (inVariantSection && line.contains(variant + " ") && line.contains(layout + ":")) {
                QStringList parts = line.split(layout + ":");
                if (parts.size() >= 2) {
                    QString description = parts[1].trimmed();
                    return description.isEmpty() ? variant : description;
                }
            }
        }
    }
    
    return variant;
}