QT += gui testlib
CONFIG += c++17 console testcase
CONFIG -= app_bundle
TARGET = edgedetection-test
INCLUDEPATH += ..
SOURCES += edgedetection_test.cpp ../edgedetection.cpp
HEADERS += ../edgedetection.h
