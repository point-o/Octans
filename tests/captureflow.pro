QT += widgets testlib
CONFIG += c++17 console
TARGET = captureflow-smoke
INCLUDEPATH += ..
SOURCES += captureflow_smoke.cpp ../mainwindow.cpp ../captureflow.cpp ../desktopcapture.cpp
HEADERS += ../mainwindow.h ../captureflow.h ../desktopcapture.h
win32: LIBS += -lgdi32 -luser32
FORMS += ../mainwindow.ui
