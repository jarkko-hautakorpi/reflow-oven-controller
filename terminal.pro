greaterThan(QT_MAJOR_VERSION, 4) {
    QT       += widgets serialport
} else {
    include($$QTSERIALPORT_PROJECT_ROOT/src/serialport/qt4support/serialport.prf)
}

TARGET = terminal
TEMPLATE = app

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    settingsdialog.cpp \
    hdlc.cpp \
    crc16_ccitt.c

HEADERS += \
    mainwindow.h \
    settingsdialog.h \
    hdlc.h \
    crc16_ccitt.h

FORMS += \
    mainwindow.ui \
    settingsdialog.ui

RESOURCES += \
    terminal.qrc
