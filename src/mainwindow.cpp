#include "mainwindow.h"
#include "installtype.h"
#include "installconfig.h"
#include <QApplication>
#include <QGridLayout>
#include <QMessageBox>
#include <QDir>
#include <QDebug>

// Global config instance
InstallConfig g_installConfig;

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    // Reset installation config to ensure clean state
    g_installConfig.reset();
    
    setupUI();
    loadData();
}

void MainWindow::setupUI() {
    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(40, 40, 40, 40);
    
    // Title
    QLabel *title = new QLabel("Xray_OS Installer", this);
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-size: 28px; font-weight: 300; color: #18e8ec; margin-bottom: 10px;");
    
    // Logo
    QLabel *logo = new QLabel(this);
    QPixmap logoPixmap(":/src/resources/icons/xray-installer.png");
    if (!logoPixmap.isNull()) {
        logo->setPixmap(logoPixmap.scaled(96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        logo->setAlignment(Qt::AlignCenter);
    }
    
    QLabel *subtitle = new QLabel("Configure System Locale", this);
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setStyleSheet("font-size: 14px; color: #888; margin-bottom: 30px;");
    
    mainLayout->addWidget(title);
    mainLayout->addWidget(logo);
    mainLayout->addWidget(subtitle);
    
    // Form layout
    QGridLayout *formLayout = new QGridLayout();
    formLayout->setSpacing(15);
    
    // Language
    formLayout->addWidget(new QLabel("Language:"), 0, 0);
    languageCombo = new QComboBox();
    formLayout->addWidget(languageCombo, 0, 1);
    
    // Region
    formLayout->addWidget(new QLabel("Region:"), 1, 0);
    regionCombo = new QComboBox();
    formLayout->addWidget(regionCombo, 1, 1);
    
    // Timezone
    formLayout->addWidget(new QLabel("Timezone:"), 2, 0);
    timezoneCombo = new QComboBox();
    formLayout->addWidget(timezoneCombo, 2, 1);
    
    // Keyboard Layout
    formLayout->addWidget(new QLabel("Keyboard Layout:"), 3, 0);
    keyboardLayoutCombo = new QComboBox();
    formLayout->addWidget(keyboardLayoutCombo, 3, 1);
    
    // Keyboard Variant
    formLayout->addWidget(new QLabel("Keyboard Variant:"), 4, 0);
    keyboardVariantCombo = new QComboBox();
    formLayout->addWidget(keyboardVariantCombo, 4, 1);
    
    // Test Input
    formLayout->addWidget(new QLabel("Test Keyboard:"), 5, 0);
    testInput = new QLineEdit();
    testInput->setPlaceholderText("Type here to test your keyboard");
    testInput->setFocus();
    formLayout->addWidget(testInput, 5, 1);
    
    mainLayout->addLayout(formLayout);
    mainLayout->addStretch();
    
    // Continue button
    continueButton = new QPushButton("Continue");
    continueButton->setStyleSheet(
        "QPushButton {"
        "  background-color: #18e8ec;"
        "  color: black;"
        "  border: none;"
        "  padding: 12px 24px;"
        "  border-radius: 6px;"
        "  font-size: 14px;"
        "  font-weight: 500;"
        "}"
        "QPushButton:hover {"
        "  background-color: #0ea5a8;"
        "}"
    );
    mainLayout->addWidget(continueButton, 0, Qt::AlignCenter);
    
    // Connect signals
    connect(regionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &MainWindow::onRegionChanged);
    connect(keyboardLayoutCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &MainWindow::onKeyboardLayoutChanged);
    connect(keyboardVariantCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &MainWindow::onKeyboardVariantChanged);
    connect(continueButton, &QPushButton::clicked, this, &MainWindow::onContinue);
    
    // Window styling
    setStyleSheet(
        "QMainWindow {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1, "
        "    stop:0 #1a1a2e, stop:0.5 #16213e, stop:1 #0f0f23);"
        "}"
        "QLabel {"
        "  color: white;"
        "  font-size: 14px;"
        "}"
        "QLineEdit {"
        "  background-color: #16213e;"
        "  border: 1px solid #18e8ec;"
        "  border-radius: 6px;"
        "  padding: 8px;"
        "  color: white;"
        "  font-size: 14px;"
        "}"
    );
}

void MainWindow::loadData() {
    // Load languages
    QStringList languages = LocaleData::loadLanguages();
    languageCombo->addItems(languages);
    
    // Load regions
    QStringList regions = LocaleData::loadRegions();
    regionCombo->addItems(regions);
    
    // Load timezone data
    timezoneData = LocaleData::loadTimezones();
    
    // Load keyboard layouts
    keyboardLayouts = LocaleData::loadKeyboardLayouts();
    for (const KeyboardLayout &layout : keyboardLayouts) {
        keyboardLayoutCombo->addItem(layout.description, layout.name);
    }
    
    // Set default selections
    int englishIndex = languages.indexOf("English");
    if (englishIndex >= 0) {
        languageCombo->setCurrentIndex(englishIndex);
    }
    
    int americaIndex = regions.indexOf("America");
    if (americaIndex >= 0) {
        regionCombo->setCurrentIndex(americaIndex);
        if (timezoneData.contains("America")) {
            timezoneCombo->addItems(timezoneData["America"]);
            int caracasIndex = timezoneData["America"].indexOf("Caracas");
            if (caracasIndex >= 0) {
                timezoneCombo->setCurrentIndex(caracasIndex);
            }
        }
    }
    
    // Set default keyboard layout to US
    for (int i = 0; i < keyboardLayouts.size(); ++i) {
        if (keyboardLayouts[i].name == "us") {
            keyboardLayoutCombo->setCurrentIndex(i);
            // Trigger variant loading
            onKeyboardLayoutChanged();
            // Set default variant to alt-intl
            for (int j = 0; j < keyboardVariantCombo->count(); ++j) {
                if (keyboardVariantCombo->itemData(j).toString() == "alt-intl") {
                    keyboardVariantCombo->setCurrentIndex(j);
                    break;
                }
            }
            break;
        }
    }
}

void MainWindow::onRegionChanged() {
    QString selectedRegion = regionCombo->currentText();
    timezoneCombo->clear();
    
    if (timezoneData.contains(selectedRegion)) {
        timezoneCombo->addItems(timezoneData[selectedRegion]);
    }
}

void MainWindow::onKeyboardLayoutChanged() {
    QString selectedLayoutName = keyboardLayoutCombo->currentData().toString();
    keyboardVariantCombo->clear();
    
    // Find the layout and populate variants
    for (const KeyboardLayout &layout : keyboardLayouts) {
        if (layout.name == selectedLayoutName) {
            for (const KeyboardVariant &variant : layout.variants) {
                keyboardVariantCombo->addItem(variant.description, variant.name);
            }
            break;
        }
    }
    
    // Auto-select first variant and apply it
    if (keyboardVariantCombo->count() > 0) {
        keyboardVariantCombo->setCurrentIndex(0);
        onKeyboardVariantChanged();
    }
}

void MainWindow::onKeyboardVariantChanged() {
    QString layoutName = keyboardLayoutCombo->currentData().toString();
    QString variantName = keyboardVariantCombo->currentData().toString();
    
    if (!layoutName.isEmpty()) {
        bool success = LocaleData::setKeyboardLayout(layoutName, variantName);
        if (success) {
            qDebug() << "Keyboard layout set to:" << layoutName << variantName;
            testInput->clear();
            testInput->setFocus();
        } else {
            qDebug() << "Failed to set keyboard layout";
        }
    }
}

void MainWindow::onContinue() {
    // Save locale settings to global config
    QString languageCode = getLanguageCode(languageCombo->currentText());
    g_installConfig.language = languageCode + ".UTF-8 UTF-8";
    g_installConfig.region = regionCombo->currentText();
    g_installConfig.timezone = regionCombo->currentText() + "/" + timezoneCombo->currentText();
    g_installConfig.keyboardLayout = keyboardLayoutCombo->currentData().toString();
    g_installConfig.keyboardVariant = keyboardVariantCombo->currentData().toString();
    
    InstallTypeWindow *installTypeWindow = new InstallTypeWindow(this);
    installTypeWindow->setWindowTitle("Arch7z Installer");
    installTypeWindow->resize(1024, 800);
    installTypeWindow->show();
    this->hide();
}

QString MainWindow::getLanguageCode(const QString &languageName) {
    static QMap<QString, QString> languageMap = {
        {"English", "en_US"},
        {"Español (Spanish)", "es_ES"},
        {"Français (French)", "fr_FR"},
        {"Deutsch (German)", "de_DE"},
        {"Italiano (Italian)", "it_IT"},
        {"Português (Portuguese)", "pt_PT"},
        {"Русский (Russian)", "ru_RU"},
        {"中文 (Chinese)", "zh_CN"},
        {"日本語 (Japanese)", "ja_JP"},
        {"한국어 (Korean)", "ko_KR"},
        {"العربية (Arabic)", "ar_SA"},
        {"हिन्दी (Hindi)", "hi_IN"},
        {"Türkçe (Turkish)", "tr_TR"},
        {"Polski (Polish)", "pl_PL"},
        {"Nederlands (Dutch)", "nl_NL"},
        {"Svenska (Swedish)", "sv_SE"},
        {"Dansk (Danish)", "da_DK"},
        {"Norsk (Norwegian)", "no_NO"},
        {"Suomi (Finnish)", "fi_FI"},
        {"Čeština (Czech)", "cs_CZ"},
        {"Magyar (Hungarian)", "hu_HU"},
        {"Română (Romanian)", "ro_RO"},
        {"Български (Bulgarian)", "bg_BG"},
        {"Hrvatski (Croatian)", "hr_HR"},
        {"Slovenčina (Slovak)", "sk_SK"},
        {"Slovenščina (Slovenian)", "sl_SI"},
        {"Eesti (Estonian)", "et_EE"},
        {"Latviešu (Latvian)", "lv_LV"},
        {"Lietuvių (Lithuanian)", "lt_LT"},
        {"Українська (Ukrainian)", "uk_UA"},
        {"Беларуская (Belarusian)", "be_BY"},
        {"Српски (Serbian)", "sr_RS"},
        {"Bosanski (Bosnian)", "bs_BA"},
        {"Македонски (Macedonian)", "mk_MK"},
        {"Shqip (Albanian)", "sq_AL"},
        {"Ελληνικά (Greek)", "el_GR"},
        {"Malti (Maltese)", "mt_MT"},
        {"Íslenska (Icelandic)", "is_IS"},
        {"Gaeilge (Irish)", "ga_IE"}
    };
    
    return languageMap.value(languageName, "en_US");
}