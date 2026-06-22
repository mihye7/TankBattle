#include <QApplication>
#include "gamewidget.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Tank Battle");
    app.setApplicationVersion("1.0");

    GameWidget game;
    game.setWindowTitle("坦克大战 - Tank Battle");
    game.setFixedSize(620, 560);
    game.show();

    return app.exec();
}
