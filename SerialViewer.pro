QT       += core gui serialport network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

TARGET = SerialViewer
TEMPLATE = app

SOURCES += \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    mainwindow.h

FORMS += \
    mainwindow.ui

TRANSLATIONS += \
    SerialViewer_en_US.ts
CONFIG += lrelease
CONFIG += embed_translations

unix {
   isEmpty(PREFIX) {
      PREFIX=/usr/local
   }
   target.path = $$PREFIX/bin
   shortcutfiles.files=SerialViewer.desktop
   shortcutfiles.path = $$PREFIX/share/applications
   INSTALLS += shortcutfiles
   DISTFILES += SerialViewer.desktop
}

iconfiles.files=icons
iconfiles.path = $$PREFIX/share/icons
INSTALLS += iconfiles

INSTALLS += target
