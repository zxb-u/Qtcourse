QT += core gui sql widgets charts

CONFIG += c++17

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

SOURCES += \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    mainwindow.h

FORMS += \
    mainwindow.ui

# Windows中文乱码修复
win32 {
    QMAKE_CXXFLAGS += -utf-8
    QMAKE_CFLAGS += -utf-8
}


# 部署规则
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
