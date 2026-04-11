#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include "notestore.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setOrganizationName("flick");
    app.setApplicationName("flick");

    QQuickStyle::setStyle("Basic");

    NoteStore store;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("noteStore", &store);
    engine.load(QUrl("qrc:/qml/main.qml"));

    if (engine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}
