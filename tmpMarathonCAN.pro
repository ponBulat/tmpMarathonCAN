QT -= gui
QT *= serialbus  # для использования QCanBus

CONFIG += c++17 console
CONFIG -= app_bundle

SOURCES += \
        main.cpp

# настройки, специфичные для windows
win32 {
    # проверим установлена ли библиотека CHAI в системе
    exists ( C:\Program Files (x86)\CHAI-2.14.0\x64\chai.dll ) {

        INCLUDEPATH *= "C:\Program Files (x86)\CHAI-2.14.0\include"
        LIBS *= -L"C:\Program Files (x86)\CHAI-2.14.0\x64" -lchai

        SOURCES += marathoncan.cpp
        HEADERS += marathoncan.h

        DEFINES *= use_Marathon

    } else {
        message( CHAI-2.14.0 not found)
    }
}
