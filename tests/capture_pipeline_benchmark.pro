QT += core gui
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = capture_pipeline_benchmark
INCLUDEPATH += ..
SOURCES += capture_pipeline_benchmark.cpp ../pixeltransform.cpp
HEADERS += ../pixeltransform.h
