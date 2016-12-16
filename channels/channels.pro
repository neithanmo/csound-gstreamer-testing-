TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.c
unix: CONFIG += link_pkgconfig
unix: PKGCONFIG += gstreamer-1.0
unix: PKGCONFIG += gstreamer-video-1.0
