#include "src/locale.h"
#include <QCoreApplication>
#include <QDebug>
#include <iostream>

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    
    auto timezones = LocaleData::loadTimezones();
    std::cout << "Total regions: " << timezones.keys().size() << std::endl;
    std::cout << "Regions: ";
    for (const QString &region : timezones.keys()) {
        std::cout << region.toStdString() << " ";
    }
    std::cout << std::endl;
    
    if (timezones.contains("Africa")) {
        std::cout << "Africa zones: " << timezones["Africa"].size() << std::endl;
        std::cout << "First 10 Africa zones: ";
        for (int i = 0; i < qMin(10, timezones["Africa"].size()); ++i) {
            std::cout << timezones["Africa"][i].toStdString() << " ";
        }
        std::cout << std::endl;
    } else {
        std::cout << "Africa not found in regions!" << std::endl;
    }
    
    return 0;
}