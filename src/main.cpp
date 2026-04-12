#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include "qml/qt_notestore.h"
#include "qml/qt_mathengine.h"
#include "qml/qt_listengine.h"
#include "qml/qt_statsengine.h"
#include "autopaste.h"
#include "syntaxhighlighter.h"
#include "markdownstyler.h"
#include "githubsync.h"
#include "windowdragger.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setOrganizationName("flick");
    app.setApplicationName("flick");

    QQuickStyle::setStyle("Basic");

    QtNoteStore store;
    QtMathEngine mathEngine;
    QtListEngine listEngine;
    QtStatsEngine statsEngine;
    AutoPaste autoPaste(&store);
    SyntaxHighlighter highlighter;
    MarkdownStyler markdownStyler;
    GitHubSync githubSync;
    WindowDragger windowDragger;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("noteStore", &store);
    engine.rootContext()->setContextProperty("mathEngine", &mathEngine);
    engine.rootContext()->setContextProperty("listEngine", &listEngine);
    engine.rootContext()->setContextProperty("statsEngine", &statsEngine);
    engine.rootContext()->setContextProperty("autoPaste", &autoPaste);
    engine.rootContext()->setContextProperty("syntaxHighlighter", &highlighter);
    engine.rootContext()->setContextProperty("markdownStyler", &markdownStyler);
    engine.rootContext()->setContextProperty("githubSync", &githubSync);
    engine.rootContext()->setContextProperty("windowDragger", &windowDragger);
    engine.load(QUrl("qrc:/qml/main.qml"));

    if (engine.rootObjects().isEmpty())
        return 1;

    QObject::connect(&githubSync, &GitHubSync::notesRestored, [&store]() {
        store.reload();
    });

    QObject::connect(&app, &QGuiApplication::aboutToQuit, [&githubSync]() {
        githubSync.syncOnQuit();
    });

    return app.exec();
}
