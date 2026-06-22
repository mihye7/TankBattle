#ifndef GAMEWIDGET_H
#define GAMEWIDGET_H

#include <QWidget>

class GameEngine;

class GameWidget : public QWidget
{
    Q_OBJECT

public:
    explicit GameWidget(QWidget *parent = nullptr);
    ~GameWidget();

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private:
    GameEngine *m_engine;

    // drawing helpers
    void drawMap(class QPainter &p);
    void drawTank(QPainter &p, const struct Tank &tank, bool isPlayer, bool invuln);
    void drawBullet(QPainter &p, const struct Bullet &bullet);
    void drawUI(QPainter &p);
    void drawOverlay(QPainter &p, const QString &title, const QString &subtitle);
    void drawBase(QPainter &p);
};

#endif // GAMEWIDGET_H
