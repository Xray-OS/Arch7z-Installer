#pragma once
#include <QString>
#include <QStringList>
#include <QMap>
#include <QProcess>

struct KeyboardVariant {
    QString name;
    QString description;
};

struct KeyboardLayout {
    QString name;
    QString description;
    QList<KeyboardVariant> variants;
};

class LocaleData {
public:
    static QStringList loadLanguages();
    static QStringList loadRegions();
    static QMap<QString, QStringList> loadTimezones();
    static QList<KeyboardLayout> loadKeyboardLayouts();
    static bool setKeyboardLayout(const QString &layout, const QString &variant);
    static QString getLanguageName(const QString &code);
    static QString getLayoutDescription(const QString &code);
    static QList<KeyboardVariant> getLayoutVariants(const QString &layout);
    static QString getVariantDisplayName(const QString &layout, const QString &variant);
};