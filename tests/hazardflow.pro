QT += widgets testlib
CONFIG += console testcase c++17
CONFIG -= app_bundle
TARGET = hazardflow-test
INCLUDEPATH += ..
SOURCES += hazardflow_test.cpp ../desktopcapture.cpp ../pixeltransform.cpp ../edgedetection.cpp
HEADERS += ../desktopcapture.h ../pixeltransform.h ../edgedetection.h
win32: LIBS += -lgdi32 -luser32
