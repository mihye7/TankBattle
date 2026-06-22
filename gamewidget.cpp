#include "gamewidget.h"
#include "gameengine.h"

#include <QPainter>
#include <QKeyEvent>
#include <QFont>
#include <QtMath>

GameWidget::GameWidget(QWidget *parent)
    : QWidget(parent)
    , m_engine(new GameEngine(this))
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(false);

    connect(m_engine, &GameEngine::updated, this, [this]() { update(); });

    // initial draw
    update();
}

GameWidget::~GameWidget()
{
}

// ============================================================
// Keyboard events
// ============================================================
void GameWidget::keyPressEvent(QKeyEvent *event)
{
    m_engine->keyPress(event->key());
    // prevent repeat flooding (we track held keys ourselves)
    if (event->isAutoRepeat()) return;
}

void GameWidget::keyReleaseEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat()) return;
    m_engine->keyRelease(event->key());
}

// ============================================================
// Paint
// ============================================================
void GameWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // background — classic dark green battlefield
    p.fillRect(rect(), QColor(0x1A, 0x1A, 0x1A));

    // sidebar background
    p.fillRect(QRect(GAME_WIDTH, 0, 20, GAME_HEIGHT), QColor(0x33, 0x33, 0x33));
    p.fillRect(QRect(0, GAME_HEIGHT, width(), 40), QColor(0x22, 0x22, 0x22));

    GameState st = m_engine->state();

    if (st == STATE_MENU && m_engine->score() == 0) {
        drawOverlay(p,
            QString::fromUtf8("坦 克 大 战"),
            "TANK BATTLE\n\n"
            "WASD / Arrow Keys — Move\n"
            "Space / J — Fire\n"
            "P / Esc — Pause\n\n"
            "Press ENTER to Start");
        return;
    }

    // draw game world
    drawMap(p);
    drawBase(p);

    // bullets (below tanks)
    for (const Bullet &b : m_engine->playerBullets())
        if (b.active) drawBullet(p, b);
    for (const Bullet &b : m_engine->enemyBullets())
        if (b.active) drawBullet(p, b);

    // enemy tanks
    for (const Tank &t : m_engine->enemies())
        if (t.alive) drawTank(p, t, false, false);

    // player tank
    const Tank &pt = m_engine->player();
    if (pt.alive) {
        bool invuln = (m_engine->invulnTimer() > 0);
        // flash during invulnerability
        if (!invuln || (m_engine->invulnTimer() / 6) % 2)
            drawTank(p, pt, true, invuln);
    }

    // UI
    drawUI(p);

    // overlay screens
    if (st == STATE_MENU) {
        drawOverlay(p, "PAUSED", "Press P or Esc to resume");
    } else if (st == STATE_WIN) {
        drawOverlay(p,
            QString::fromUtf8("胜 利 ！"),
            QString("Final Score: %1\n\nPress ENTER to play again").arg(m_engine->score()));
    } else if (st == STATE_LOSE) {
        drawOverlay(p,
            QString::fromUtf8("游 戏 结 束"),
            QString("Score: %1   Level: %2\n\nPress ENTER to try again")
                .arg(m_engine->score()).arg(m_engine->level()));
    }
}

// ============================================================
// Map rendering
// ============================================================
void GameWidget::drawMap(QPainter &p)
{
    for (int r = 0; r < MAP_ROWS; ++r) {
        for (int c = 0; c < MAP_COLS; ++c) {
            int cell = m_engine->cellType(r, c);
            if (cell == CELL_EMPTY || cell == CELL_BASE) continue;

            int x = c * CELL_SIZE;
            int y = r * CELL_SIZE;

            if (cell == CELL_BRICK) {
                // brick wall — orange-brown with mortar lines
                p.fillRect(x, y, CELL_SIZE, CELL_SIZE, QColor(0xC4, 0x71, 0x3B));
                p.setPen(QPen(QColor(0x8B, 0x45, 0x13), 1));
                // horizontal mortar
                p.drawLine(x, y + CELL_SIZE/2, x + CELL_SIZE, y + CELL_SIZE/2);
                // vertical mortar (offset per row)
                for (int i = 0; i < CELL_SIZE; i += CELL_SIZE / 2) {
                    int ox = (r % 2 == 0) ? 0 : CELL_SIZE / 4;
                    p.drawLine(x + ox + CELL_SIZE/2, y + i,
                               x + ox + CELL_SIZE/2, y + i + CELL_SIZE/2);
                }
                // border
                p.setPen(QPen(QColor(0x5D, 0x2E, 0x0C), 1));
                p.drawRect(x, y, CELL_SIZE - 1, CELL_SIZE - 1);

            } else if (cell == CELL_STEEL) {
                // steel wall — silver/gray
                p.fillRect(x, y, CELL_SIZE, CELL_SIZE, QColor(0x9E, 0x9E, 0x9E));
                p.fillRect(x + 4, y + 4, CELL_SIZE - 8, CELL_SIZE - 8,
                           QColor(0xBD, 0xBD, 0xBD));
                // rivets
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(0x75, 0x75, 0x75));
                int d = CELL_SIZE - 1;
                p.drawEllipse(QPointF(x + 6, y + 6), 3, 3);
                p.drawEllipse(QPointF(x + d - 5, y + 6), 3, 3);
                p.drawEllipse(QPointF(x + 6, y + d - 5), 3, 3);
                p.drawEllipse(QPointF(x + d - 5, y + d - 5), 3, 3);
                p.setPen(QPen(QColor(0x61, 0x61, 0x61), 2));
                p.drawRect(x, y, CELL_SIZE - 1, CELL_SIZE - 1);
            }
        }
    }
}

// ============================================================
// Base (Eagle) rendering
// ============================================================
void GameWidget::drawBase(QPainter &p)
{
    QPointF bp = m_engine->basePos();
    int bx = static_cast<int>(bp.x());
    int by = static_cast<int>(bp.y());

    // dark background
    p.fillRect(bx, by, CELL_SIZE, CELL_SIZE, QColor(0x2C, 0x2C, 0x2C));
    p.setPen(QPen(QColor(0x55, 0x55, 0x55), 1));
    p.drawRect(bx, by, CELL_SIZE - 1, CELL_SIZE - 1);

    // eagle shape (simplified)
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0xFF, 0xD7, 0x00));   // gold

    int cx = bx + CELL_SIZE / 2;
    int cy = by + CELL_SIZE / 2;

    // body
    QPointF body[] = {
        QPointF(cx-8, cy+5), QPointF(cx+8, cy+5),
        QPointF(cx+6, cy-3), QPointF(cx+2, cy-3),
        QPointF(cx+2, cy-10), QPointF(cx-2, cy-10),
        QPointF(cx-2, cy-3), QPointF(cx-6, cy-3),
    };
    p.drawPolygon(body, 8);

    // wings
    QPointF lwing[] = {
        QPointF(cx-8, cy-7), QPointF(cx-6, cy-3),
        QPointF(cx-14, cy+5), QPointF(cx-8, cy),
    };
    QPointF rwing[] = {
        QPointF(cx+8, cy-7), QPointF(cx+6, cy-3),
        QPointF(cx+14, cy+5), QPointF(cx+8, cy),
    };
    p.drawPolygon(lwing, 4);
    p.drawPolygon(rwing, 4);
}

// ============================================================
// Tank rendering
// ============================================================
void GameWidget::drawTank(QPainter &p, const Tank &tank, bool isPlayer, bool invuln)
{
    int tx = static_cast<int>(tank.x);
    int ty = static_cast<int>(tank.y);
    int sz = TANK_SIZE;
    int cx = tx + sz / 2;
    int cy = ty + sz / 2;

    // colour selection
    QColor bodyC, barrelC, treadC, turretC;
    if (isPlayer) {
        bodyC   = invuln ? QColor(0xFF, 0xD7, 0x00) : QColor(0x4C, 0xAF, 0x50);
        barrelC = invuln ? QColor(0xFF, 0xA0, 0x00) : QColor(0x2E, 0x7D, 0x32);
        treadC  = invuln ? QColor(0xCC, 0x88, 0x00) : QColor(0x1B, 0x5E, 0x20);
        turretC = invuln ? QColor(0xFF, 0xE0, 0x80) : QColor(0x81, 0xC7, 0x84);
    } else {
        switch (tank.enemyType) {
        case ENEMY_FAST:
            bodyC   = QColor(0x42, 0xA5, 0xF5);
            barrelC = QColor(0x15, 0x65, 0xC0);
            treadC  = QColor(0x0D, 0x47, 0xA1);
            turretC = QColor(0x90, 0xCA, 0xF9);
            break;
        case ENEMY_HEAVY:
            bodyC   = (tank.health > 1) ? QColor(0xAB, 0x47, 0xBC) : QColor(0xCE, 0x93, 0xD8);
            barrelC = QColor(0x6A, 0x1B, 0x9A);
            treadC  = QColor(0x4A, 0x14, 0x8C);
            turretC = QColor(0xE1, 0xBE, 0xE7);
            break;
        default: // NORMAL
            bodyC   = QColor(0xEF, 0x53, 0x50);
            barrelC = QColor(0xC6, 0x28, 0x28);
            treadC  = QColor(0x8B, 0x00, 0x00);
            turretC = QColor(0xFF, 0x8A, 0x80);
            break;
        }
    }

    // damage overlay for heavy tanks that are damaged
    if (!isPlayer && tank.enemyType == ENEMY_HEAVY && tank.health <= 1)
        bodyC = bodyC.lighter(140);

    // --- body ---
    p.setPen(QPen(bodyC.darker(130), 2));
    p.setBrush(bodyC);
    p.drawRoundedRect(tx + 1, ty + 1, sz - 2, sz - 2, 4, 4);

    // --- treads ---
    p.setPen(Qt::NoPen);
    p.setBrush(treadC);
    if (tank.dir == DIR_UP || tank.dir == DIR_DOWN) {
        p.drawRect(tx + 1, ty + 2, 5, sz - 4);
        p.drawRect(tx + sz - 6, ty + 2, 5, sz - 4);
    } else {
        p.drawRect(tx + 2, ty + 1, sz - 4, 5);
        p.drawRect(tx + 2, ty + sz - 6, sz - 4, 5);
    }

    // --- turret (centre circle) ---
    p.setBrush(turretC);
    p.setPen(QPen(turretC.darker(120), 1));
    p.drawEllipse(QPointF(cx, cy), 6, 6);

    // --- barrel ---
    p.setPen(Qt::NoPen);
    p.setBrush(barrelC);
    int bw = 6;   // barrel width
    int bl = 14;  // barrel length
    switch (tank.dir) {
    case DIR_UP:
        p.drawRect(cx - bw/2, ty - bl, bw, bl);
        break;
    case DIR_DOWN:
        p.drawRect(cx - bw/2, ty + sz, bw, bl);
        break;
    case DIR_LEFT:
        p.drawRect(tx - bl, cy - bw/2, bl, bw);
        break;
    case DIR_RIGHT:
        p.drawRect(tx + sz, cy - bw/2, bl, bw);
        break;
    }
}

// ============================================================
// Bullet rendering
// ============================================================
void GameWidget::drawBullet(QPainter &p, const Bullet &bullet)
{
    p.setPen(Qt::NoPen);

    // glow
    p.setBrush(QColor(0xFF, 0xFF, 0xCC, 80));
    p.drawEllipse(QPointF(bullet.x + BULLET_SIZE/2.0, bullet.y + BULLET_SIZE/2.0),
                  BULLET_SIZE, BULLET_SIZE);

    // core
    p.setBrush(QColor(0xFF, 0xFD, 0xE7));
    p.drawEllipse(QPointF(bullet.x + BULLET_SIZE/2.0, bullet.y + BULLET_SIZE/2.0),
                  BULLET_SIZE/2.0, BULLET_SIZE/2.0);
}

// ============================================================
// HUD
// ============================================================
void GameWidget::drawUI(QPainter &p)
{
    // bottom info bar
    p.setPen(QColor(0xCC, 0xCC, 0xCC));
    QFont f("Consolas", 10);
    f.setBold(true);
    p.setFont(f);

    int barY = GAME_HEIGHT + 10;
    p.drawText(10, barY, 22,
               QString("LEVEL %1").arg(m_engine->level()));

    p.drawText(120, barY, 22,
               QString("SCORE %1").arg(m_engine->score()));

    p.drawText(260, barY, 22,
               QString::fromUtf8("LIVES %1").arg(m_engine->lives()));

    // enemy counter
    QString enemyStr = QString::fromUtf8("ENEMIES %1/%2")
        .arg(m_engine->enemiesKilled()).arg(m_engine->enemiesToSpawn());
    p.drawText(400, barY, 22, enemyStr);

    // freeze indicator
    if (m_engine->isFrozen()) {
        p.setPen(QColor(0x4F, 0xC3, 0xF7));
        p.drawText(520, barY, 22, "FREEZE!");
    }

    // minimap label
    f.setPointSize(8);
    p.setFont(f);
    p.setPen(QColor(0x88, 0x88, 0x88));
    p.drawText(GAME_WIDTH + 2, 12, 16, "T\nA\nN\nK\n\nB\nA\nT\nT\nL\nE");
}

// ============================================================
// Overlay screens
// ============================================================
void GameWidget::drawOverlay(QPainter &p, const QString &title, const QString &subtitle)
{
    // semi-transparent backdrop
    p.fillRect(0, 0, GAME_WIDTH, GAME_HEIGHT, QColor(0, 0, 0, 180));

    // title
    QFont tf("Microsoft YaHei", 28, QFont::Bold);
    p.setFont(tf);
    p.setPen(QColor(0xFF, 0xD7, 0x00));
    QRectF tr(0, GAME_HEIGHT/2 - 80, GAME_WIDTH, 50);
    p.drawText(tr, Qt::AlignCenter, title);

    // subtitle
    QFont sf("Consolas", 13);
    p.setFont(sf);
    p.setPen(QColor(0xCC, 0xCC, 0xCC));
    QRectF sr(0, GAME_HEIGHT/2 - 10, GAME_WIDTH, 120);
    p.drawText(sr, Qt::AlignHCenter | Qt::AlignTop, subtitle);
}
