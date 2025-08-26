#include <QApplication>
#include <QPalette>
#include <QIcon>
#include "mainwindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    // Set application icon
    app.setWindowIcon(QIcon(":/src/resources/icons/xray-installer.png"));
    
    // Force dark theme for sudo compatibility
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(26, 26, 46));
    darkPalette.setColor(QPalette::WindowText, Qt::white);
    darkPalette.setColor(QPalette::Base, QColor(22, 33, 62));
    darkPalette.setColor(QPalette::AlternateBase, QColor(26, 26, 46));
    darkPalette.setColor(QPalette::Text, Qt::white);
    darkPalette.setColor(QPalette::Button, QColor(26, 26, 46));
    darkPalette.setColor(QPalette::ButtonText, Qt::white);
    darkPalette.setColor(QPalette::Link, QColor(24, 232, 236));
    darkPalette.setColor(QPalette::Highlight, QColor(24, 232, 236));
    darkPalette.setColor(QPalette::HighlightedText, Qt::black);
    app.setPalette(darkPalette);
    
    MainWindow window;
    window.setWindowTitle("Arch7z Installer");
    window.resize(1024, 800);
    window.show();
    
    return app.exec();
}