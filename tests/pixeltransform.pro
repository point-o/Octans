QT += gui testlib
CONFIG += c++17 console testcase
CONFIG -= app_bundle
TARGET = pixeltransform-test
INCLUDEPATH += ..
SOURCES += pixeltransform_test.cpp ../pixeltransform.cpp
HEADERS += ../pixeltransform.h
