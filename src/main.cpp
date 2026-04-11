#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include "notestore.h"
#include "mathengine.h"
#include "autopaste.h"
#include "syntaxhighlighter.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setOrganizationName("flick");
    app.setApplicationName("flick");

    QQuickStyle::setStyle("Basic");

    NoteStore store;
    MathEngine mathEngine;
    AutoPaste autoPaste(&store);
    SyntaxHighlighter highlighter;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("noteStore", &store);
    engine.rootContext()->setContextProperty("mathEngine", &mathEngine);
    engine.rootContext()->setContextProperty("autoPaste", &autoPaste);
    engine.rootContext()->setContextProperty("syntaxHighlighter", &highlighter);
    engine.load(QUrl("qrc:/qml/main.qml"));

    if (engine.rootObjects().isEmpty())
        return 1;

    return app.exec();
}
