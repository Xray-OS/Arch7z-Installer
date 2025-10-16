cmake_minimum_required(VERSION 3.16)
project(arch7z-installer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

qt6_standard_project_setup()

set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/installtype.cpp
    src/diskselection.cpp
    src/partitionlayout.cpp
    src/vmpartitionlayout.cpp
    src/advancedpartition.cpp
    src/userconfig.cpp
    src/locale.cpp
    src/installprogress.cpp
    src/installer.cpp
    src/vmdetection.cpp
    src/vminstaller.cpp
    src/settingsparser.cpp
)

set(HEADERS
    src/mainwindow.h
    src/installtype.h
    src/diskselection.h
    src/partitionlayout.h
    src/vmpartitionlayout.h
    src/advancedpartition.h
    src/userconfig.h
    src/locale.h
    src/installprogress.h
    src/installer.h
    src/installconfig.h
    src/vmdetection.h
    src/vminstaller.h
    src/settingsparser.h
)

qt6_add_executable(arch7z-installer ${SOURCES} ${HEADERS})
qt6_add_resources(arch7z-installer "resources" PREFIX "/" FILES src/resources/icons/xray-installer.png)

target_link_libraries(arch7z-installer PRIVATE Qt6::Core Qt6::Widgets)