#include "gameengine.h"

#include <QtMath>
#include <QRandomGenerator>
#include <algorithm>

// helper — thread-safe bounded random
static inline int rnd(int n)
{
    return QRandomGenerator::global()->bounded(n);
}

// ============================================================
// Construction
// ============================================================
GameEngine::GameEngine(QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
    , m_state(STATE_MENU)
    , m_score(0)
    , m_lives(PLAYER_LIVES)
    , m_level(1)
    , m_invulnTimer(0)
    , m_freezeTimer(0)
    , m_enemiesToSpawn(0)
    , m_enemiesKilled(0)
    , m_spawnTimer(0)
{
    connect(m_timer, &QTimer::timeout, this, &GameEngine::tick);
    m_timer->setInterval(16);          // ≈ 60 FPS

    // spawn points — top-left, top-centre, top-right
    m_spawnPoints << QPointF(0 * CELL_SIZE + 3, 0 * CELL_SIZE + 3);
    m_spawnPoints << QPointF(7 * CELL_SIZE + 3, 0 * CELL_SIZE + 3);
    m_spawnPoints << QPointF(14 * CELL_SIZE + 3, 0 * CELL_SIZE + 3);

    // empty map
    for (int r = 0; r < MAP_ROWS; ++r)
        for (int c = 0; c < MAP_COLS; ++c)
            m_map[r][c] = CELL_EMPTY;

    // init player
    QPointF sp = playerSpawnPos();
    m_player.x            = sp.x();
    m_player.y            = sp.y();
    m_player.dir          = DIR_UP;
    m_player.speed        = PLAYER_SPEED;
    m_player.health       = 1;
    m_player.maxHealth    = 1;
    m_player.alive        = true;
    m_player.shootCooldown= 0;
    m_player.moveTimer    = 0;
    m_player.isPlayer     = true;

    loadLevel(1);
}

// ============================================================
// Public API
// ============================================================
void GameEngine::startGame()
{
    m_score  = 0;
    m_lives  = PLAYER_LIVES;
    m_level  = 1;
    m_state  = STATE_PLAYING;
    m_invulnTimer = 60;
    resetGame();
    m_timer->start();
}

void GameEngine::resetGame()
{
    m_playerBullets.clear();
    m_enemyBullets.clear();
    m_enemies.clear();
    m_freezeTimer = 0;
    loadLevel(m_level);
}

void GameEngine::togglePause()
{
    if (m_state == STATE_PLAYING) {
        m_state = STATE_MENU;   // reuse MENU as "paused" during play
        m_timer->stop();
    } else if (m_state == STATE_MENU) {
        m_state = STATE_PLAYING;
        m_timer->start();
    }
}

// ============================================================
// Input
// ============================================================
void GameEngine::keyPress(int key)
{
    m_keysHeld.insert(key);

    // start / restart
    if (key == Qt::Key_Return || key == Qt::Key_Enter) {
        if (m_state == STATE_MENU || m_state == STATE_WIN || m_state == STATE_LOSE) {
            startGame();
            return;
        }
    }

    // pause
    if (key == Qt::Key_P || key == Qt::Key_Escape) {
        if (m_state == STATE_PLAYING || m_state == STATE_MENU) {
            togglePause();
        }
    }
}

void GameEngine::keyRelease(int key)
{
    m_keysHeld.remove(key);
}

// ============================================================
// Main game loop
// ============================================================
void GameEngine::tick()
{
    if (m_state != STATE_PLAYING) return;

    // cooldowns
    if (m_invulnTimer > 0)            --m_invulnTimer;
    if (m_freezeTimer > 0)            --m_freezeTimer;
    if (m_player.shootCooldown > 0)   --m_player.shootCooldown;
    for (Tank &e : m_enemies)
        if (e.shootCooldown > 0)      --e.shootCooldown;

    // update world
    updatePlayer();
    updateEnemies();
    updateBullets(m_playerBullets);
    updateBullets(m_enemyBullets);
    checkCollisions();

    // spawn enemies
    if (m_enemies.size() < MAX_ON_SCREEN && m_enemiesKilled < m_enemiesToSpawn) {
        m_spawnTimer += 16;
        if (m_spawnTimer >= 3000) {
            m_spawnTimer = 0;
            spawnEnemy();
        }
    }

    // win condition
    if (m_enemiesKilled >= m_enemiesToSpawn && m_enemies.isEmpty()) {
        if (m_level < 3) {
            ++m_level;
            m_score += 500;
            loadLevel(m_level);
        } else {
            m_state = STATE_WIN;
            m_timer->stop();
        }
    }

    emit updated();
}

// ============================================================
// Player update
// ============================================================
void GameEngine::updatePlayer()
{
    if (!m_player.alive) return;

    // --- shooting ---
    if (m_keysHeld.contains(Qt::Key_Space) || m_keysHeld.contains(Qt::Key_J)) {
        if (m_player.shootCooldown == 0)
            playerShoot();
    }

    // --- direction ---
    bool wantMove = false;
    Direction newDir = m_player.dir;

    if (m_keysHeld.contains(Qt::Key_W)     || m_keysHeld.contains(Qt::Key_Up))    { newDir = DIR_UP;    wantMove = true; }
    else if (m_keysHeld.contains(Qt::Key_S)|| m_keysHeld.contains(Qt::Key_Down))  { newDir = DIR_DOWN;  wantMove = true; }
    else if (m_keysHeld.contains(Qt::Key_A)|| m_keysHeld.contains(Qt::Key_Left))  { newDir = DIR_LEFT;  wantMove = true; }
    else if (m_keysHeld.contains(Qt::Key_D)|| m_keysHeld.contains(Qt::Key_Right)) { newDir = DIR_RIGHT; wantMove = true; }

    m_player.dir = newDir;

    if (!wantMove) return;

    // --- movement ---
    qreal nx = m_player.x;
    qreal ny = m_player.y;
    switch (m_player.dir) {
    case DIR_UP:    ny -= m_player.speed; break;
    case DIR_DOWN:  ny += m_player.speed; break;
    case DIR_LEFT:  nx -= m_player.speed; break;
    case DIR_RIGHT: nx += m_player.speed; break;
    }

    QRectF tr = tankRect(nx, ny);
    if (canMoveTo(tr, &m_player)) {
        m_player.x = nx;
        m_player.y = ny;
    }
}

// ============================================================
// Enemy AI
// ============================================================
void GameEngine::updateEnemies()
{
    if (m_freezeTimer > 0) return;   // frozen — skip

    for (Tank &enemy : m_enemies) {
        if (!enemy.alive) continue;

        // --- shooting ---
        if (enemy.shootCooldown <= 0) {
            int chance = 2;               // base 2 % per frame
            // align with player → more aggressive
            if ((enemy.dir == DIR_UP || enemy.dir == DIR_DOWN) &&
                 qAbs(enemy.x - m_player.x) < TANK_SIZE + 4) chance = 15;
            if ((enemy.dir == DIR_LEFT || enemy.dir == DIR_RIGHT) &&
                 qAbs(enemy.y - m_player.y) < TANK_SIZE + 4) chance = 15;
            if (rnd(100) < chance)
                enemyShoot(enemy);
        }

        // --- movement ---
        --enemy.moveTimer;
        bool blocked = false;

        qreal tx = enemy.x, ty = enemy.y;
        switch (enemy.dir) {
        case DIR_UP:    ty -= enemy.speed; break;
        case DIR_DOWN:  ty += enemy.speed; break;
        case DIR_LEFT:  tx -= enemy.speed; break;
        case DIR_RIGHT: tx += enemy.speed; break;
        }
        blocked = !canMoveTo(tankRect(tx, ty), &enemy);

        if (blocked || enemy.moveTimer <= 0) {
            // pick a new direction (biased toward player)
            Direction best = enemy.dir;
            int bestScore  = -1;

            for (int d = 0; d < 4; ++d) {
                Direction nd = static_cast<Direction>(d);
                qreal sx = enemy.x, sy = enemy.y;
                switch (nd) {
                case DIR_UP:    sy -= enemy.speed; break;
                case DIR_DOWN:  sy += enemy.speed; break;
                case DIR_LEFT:  sx -= enemy.speed; break;
                case DIR_RIGHT: sx += enemy.speed; break;
                }

                if (!canMoveTo(tankRect(sx, sy), &enemy))
                    continue;

                // score: favour direction toward player
                int sc = 1;
                qreal dx = m_player.x - enemy.x;
                qreal dy = m_player.y - enemy.y;
                if      (nd == DIR_UP    && dy < 0) sc += 3;
                else if (nd == DIR_DOWN  && dy > 0) sc += 3;
                else if (nd == DIR_LEFT  && dx < 0) sc += 3;
                else if (nd == DIR_RIGHT && dx > 0) sc += 3;
                if (nd != enemy.dir) sc -= 1;   // slight inertia

                if (sc > bestScore) { bestScore = sc; best = nd; }
            }

            if (bestScore >= 0) {
                enemy.dir = best;
            }
            enemy.moveTimer = 40 + rnd(80);
        }

        // apply movement
        qreal nx = enemy.x, ny = enemy.y;
        switch (enemy.dir) {
        case DIR_UP:    ny -= enemy.speed; break;
        case DIR_DOWN:  ny += enemy.speed; break;
        case DIR_LEFT:  nx -= enemy.speed; break;
        case DIR_RIGHT: nx += enemy.speed; break;
        }
        QRectF nr = tankRect(nx, ny);
        if (canMoveTo(nr, &enemy)) {
            enemy.x = nx;
            enemy.y = ny;
        } else {
            enemy.moveTimer = 0;   // force re-evaluate next frame
        }
    }
}

// ============================================================
// Bullet movement
// ============================================================
void GameEngine::updateBullets(QList<Bullet> &bullets)
{
    for (Bullet &b : bullets) {
        if (!b.active) continue;
        switch (b.dir) {
        case DIR_UP:    b.y -= b.speed; break;
        case DIR_DOWN:  b.y += b.speed; break;
        case DIR_LEFT:  b.x -= b.speed; break;
        case DIR_RIGHT: b.x += b.speed; break;
        }
    }
}

// ============================================================
// Collision detection
// ============================================================
void GameEngine::checkCollisions()
{
    // --- player bullets ---
    for (Bullet &b : m_playerBullets) {
        if (!b.active) continue;

        QRectF br = bulletRect(b);
        int cx = static_cast<int>((b.x + BULLET_SIZE/2.0) / CELL_SIZE);
        int cy = static_cast<int>((b.y + BULLET_SIZE/2.0) / CELL_SIZE);

        // off screen
        if (br.left() < 0 || br.right() > GAME_WIDTH ||
            br.top() < 0  || br.bottom() > GAME_HEIGHT) {
            b.active = false; continue;
        }

        // wall / base
        if (cy >= 0 && cy < MAP_ROWS && cx >= 0 && cx < MAP_COLS) {
            int cell = m_map[cy][cx];
            if (cell == CELL_BRICK) {
                m_map[cy][cx] = CELL_EMPTY;
                b.active = false; continue;
            } else if (cell == CELL_STEEL) {
                b.active = false; continue;
            } else if (cell == CELL_BASE) {
                b.active = false;
                m_state = STATE_LOSE;
                m_timer->stop();
                continue;
            }
        }

        // enemy tank hit
        for (Tank &enemy : m_enemies) {
            if (!enemy.alive) continue;
            if (br.intersects(tankRect(enemy))) {
                b.active = false;
                --enemy.health;
                if (enemy.health <= 0) {
                    enemy.alive = false;
                    m_score += (enemy.enemyType == ENEMY_HEAVY) ? 300 :
                               (enemy.enemyType == ENEMY_FAST)  ? 200 : 100;
                    ++m_enemiesKilled;
                }
                break;
            }
        }
    }

    // --- enemy bullets ---
    for (Bullet &b : m_enemyBullets) {
        if (!b.active) continue;

        QRectF br = bulletRect(b);
        int cx = static_cast<int>((b.x + BULLET_SIZE/2.0) / CELL_SIZE);
        int cy = static_cast<int>((b.y + BULLET_SIZE/2.0) / CELL_SIZE);

        // off screen
        if (br.left() < 0 || br.right() > GAME_WIDTH ||
            br.top() < 0  || br.bottom() > GAME_HEIGHT) {
            b.active = false; continue;
        }

        // wall / base
        if (cy >= 0 && cy < MAP_ROWS && cx >= 0 && cx < MAP_COLS) {
            int cell = m_map[cy][cx];
            if (cell == CELL_BRICK) {
                m_map[cy][cx] = CELL_EMPTY;
                b.active = false; continue;
            } else if (cell == CELL_STEEL) {
                b.active = false; continue;
            } else if (cell == CELL_BASE) {
                b.active = false;
                m_state = STATE_LOSE;
                m_timer->stop();
                continue;
            }
        }

        // player hit
        if (m_player.alive && m_invulnTimer <= 0) {
            if (br.intersects(tankRect(m_player))) {
                b.active = false;
                destroyPlayer();
            }
        }
    }

    // --- bullet vs bullet ---
    for (Bullet &pb : m_playerBullets) {
        if (!pb.active) continue;
        QRectF pbr = bulletRect(pb);
        for (Bullet &eb : m_enemyBullets) {
            if (!eb.active) continue;
            if (pbr.intersects(bulletRect(eb))) {
                pb.active = false;
                eb.active = false;
            }
        }
    }

    // garbage-collect inactive bullets
    auto purge = [](QList<Bullet> &list) {
        list.erase(std::remove_if(list.begin(), list.end(),
                   [](const Bullet &b) { return !b.active; }), list.end());
    };
    purge(m_playerBullets);
    purge(m_enemyBullets);

    // garbage-collect dead enemies
    m_enemies.erase(std::remove_if(m_enemies.begin(), m_enemies.end(),
                    [](const Tank &t) { return !t.alive; }), m_enemies.end());
}

// ============================================================
// Shooting
// ============================================================
void GameEngine::playerShoot()
{
    int active = 0;
    for (const Bullet &b : m_playerBullets)
        if (b.active) ++active;
    if (active >= 2) return;

    Bullet b;
    b.dir    = m_player.dir;
    b.speed  = BULLET_SPEED;
    b.active = true;

    qreal cx = m_player.x + TANK_SIZE / 2.0;
    qreal cy = m_player.y + TANK_SIZE / 2.0;

    switch (b.dir) {
    case DIR_UP:    b.x = cx - BULLET_SIZE/2.0;  b.y = m_player.y - BULLET_SIZE; break;
    case DIR_DOWN:  b.x = cx - BULLET_SIZE/2.0;  b.y = m_player.y + TANK_SIZE;   break;
    case DIR_LEFT:  b.x = m_player.x - BULLET_SIZE;  b.y = cy - BULLET_SIZE/2.0; break;
    case DIR_RIGHT: b.x = m_player.x + TANK_SIZE;    b.y = cy - BULLET_SIZE/2.0; break;
    }

    m_playerBullets.append(b);
    m_player.shootCooldown = SHOOT_COOLDOWN;
}

void GameEngine::enemyShoot(Tank &enemy)
{
    // limit enemy bullets per tank
    int cnt = 0;
    for (const Bullet &b : m_enemyBullets)
        if (b.active) ++cnt;
    if (cnt >= static_cast<int>(m_enemies.size()) + 1) return;   // loose limit

    Bullet b;
    b.dir    = enemy.dir;
    b.speed  = BULLET_SPEED;
    b.active = true;

    qreal cx = enemy.x + TANK_SIZE / 2.0;
    qreal cy = enemy.y + TANK_SIZE / 2.0;

    switch (b.dir) {
    case DIR_UP:    b.x = cx - BULLET_SIZE/2.0;  b.y = enemy.y - BULLET_SIZE; break;
    case DIR_DOWN:  b.x = cx - BULLET_SIZE/2.0;  b.y = enemy.y + TANK_SIZE;   break;
    case DIR_LEFT:  b.x = enemy.x - BULLET_SIZE;  b.y = cy - BULLET_SIZE/2.0; break;
    case DIR_RIGHT: b.x = enemy.x + TANK_SIZE;    b.y = cy - BULLET_SIZE/2.0; break;
    }

    m_enemyBullets.append(b);
    enemy.shootCooldown = ENEMY_COOLDOWN + rnd(40);
}

// ============================================================
// Spawning
// ============================================================
void GameEngine::spawnEnemy()
{
    if (m_enemiesKilled >= m_enemiesToSpawn) return;

    // pick random spawn point that isn't blocked
    QList<int> indices;
    for (int i = 0; i < m_spawnPoints.size(); ++i) indices.append(i);
    // shuffle
    for (int i = indices.size() - 1; i > 0; --i)
        qSwap(indices[i], indices[rnd(i + 1)]);

    QPointF sp;
    bool found = false;
    for (int idx : indices) {
        sp = m_spawnPoints[idx];
        QRectF tr = tankRect(sp.x(), sp.y());
        bool blocked = false;
        if (m_player.alive && tr.intersects(tankRect(m_player))) blocked = true;
        for (const Tank &e : m_enemies)
            if (e.alive && tr.intersects(tankRect(e))) { blocked = true; break; }
        if (checkWallCollision(tr, nullptr)) blocked = true;
        if (!blocked) { found = true; break; }
    }
    if (!found) return;   // all occupied — try later

    Tank enemy;
    enemy.x         = sp.x();
    enemy.y         = sp.y();
    enemy.dir       = DIR_DOWN;
    enemy.speed     = ENEMY_SPEED;
    enemy.alive     = true;
    enemy.isPlayer  = false;
    enemy.shootCooldown = 30;
    enemy.moveTimer = 30;

    // random type
    int roll = rnd(100);
    if (roll < 15) {
        enemy.enemyType = ENEMY_FAST;
        enemy.speed     = 4;
        enemy.health    = 1;
        enemy.maxHealth = 1;
    } else if (roll < 25) {
        enemy.enemyType = ENEMY_HEAVY;
        enemy.speed     = 1;
        enemy.health    = 2;
        enemy.maxHealth = 2;
    } else {
        enemy.enemyType = ENEMY_NORMAL;
        enemy.speed     = ENEMY_SPEED;
        enemy.health    = 1;
        enemy.maxHealth = 1;
    }

    m_enemies.append(enemy);
}

// ============================================================
// Player death
// ============================================================
void GameEngine::destroyPlayer()
{
    m_player.alive = false;
    --m_lives;

    if (m_lives <= 0) {
        m_state = STATE_LOSE;
        m_timer->stop();
    } else {
        // respawn
        QPointF sp = playerSpawnPos();
        m_player.x = sp.x();
        m_player.y = sp.y();
        m_player.dir = DIR_UP;
        m_player.alive = true;
        m_player.shootCooldown = 30;
        m_invulnTimer = INVULN_FRAMES;
    }
}

// ============================================================
// Movement helpers
// ============================================================
QRectF GameEngine::tankRect(qreal x, qreal y) const
{
    return QRectF(x, y, TANK_SIZE, TANK_SIZE);
}

QRectF GameEngine::bulletRect(const Bullet &b) const
{
    return QRectF(b.x, b.y, BULLET_SIZE, BULLET_SIZE);
}

bool GameEngine::checkWallCollision(const QRectF &rect, const Tank * /*exclude*/) const
{
    int sc = qMax(0,       static_cast<int>(rect.left()   / CELL_SIZE));
    int ec = qMin(MAP_COLS-1, static_cast<int>((rect.right() - 0.01)  / CELL_SIZE));
    int sr = qMax(0,       static_cast<int>(rect.top()    / CELL_SIZE));
    int er = qMin(MAP_ROWS-1, static_cast<int>((rect.bottom()-0.01) / CELL_SIZE));

    for (int r = sr; r <= er; ++r)
        for (int c = sc; c <= ec; ++c)
            if (m_map[r][c] == CELL_BRICK || m_map[r][c] == CELL_STEEL ||
                m_map[r][c] == CELL_BASE)
                return true;
    return false;
}

bool GameEngine::canMoveTo(const QRectF &rect, const Tank *exclude) const
{
    // boundaries
    if (rect.left() < 0 || rect.right()  > GAME_WIDTH ||
        rect.top()  < 0 || rect.bottom() > GAME_HEIGHT)
        return false;

    // walls / base
    if (checkWallCollision(rect, exclude))
        return false;

    // player
    if (m_player.alive && (!exclude || exclude != &m_player)) {
        if (rect.intersects(tankRect(m_player)))
            return false;
    }

    // other enemies
    for (const Tank &e : m_enemies) {
        if (!e.alive) continue;
        if (exclude == &e) continue;
        if (rect.intersects(tankRect(e)))
            return false;
    }

    return true;
}

QPointF GameEngine::playerSpawnPos() const
{
    // bottom row, left-of-centre
    return QPointF(4 * CELL_SIZE + (CELL_SIZE - TANK_SIZE) / 2.0,
                   12 * CELL_SIZE + (CELL_SIZE - TANK_SIZE) / 2.0);
}

// ============================================================
// Level loading
// ============================================================
void GameEngine::loadLevel(int level)
{
    // clear map
    for (int r = 0; r < MAP_ROWS; ++r)
        for (int c = 0; c < MAP_COLS; ++c)
            m_map[r][c] = CELL_EMPTY;

    m_enemies.clear();
    m_playerBullets.clear();
    m_enemyBullets.clear();

    // ---- level definitions (each string = 15 chars) ----
    static const char *LEVEL_1[MAP_ROWS] = {
        /*00*/ "...............",
        /*01*/ "...............",
        /*02*/ "..###...###....",
        /*03*/ "..#.......#...",
        /*04*/ "..#..###..#...",
        /*05*/ "..#.......#...",
        /*06*/ "..###...###...",
        /*07*/ "...............",
        /*08*/ "..###.......##",
        /*09*/ "..#.........#.",
        /*10*/ "..#..###...#..",
        /*11*/ "..#..B.#...#..",
        /*12*/ "..............."
    };

    static const char *LEVEL_2[MAP_ROWS] = {
        /*00*/ "..###...###....",
        /*01*/ "..#.......#...",
        /*02*/ "..#..@.@..#...",
        /*03*/ "..#.......#...",
        /*04*/ "..###...###...",
        /*05*/ "...............",
        /*06*/ ".###...###....",
        /*07*/ ".#.........#..",
        /*08*/ ".#..###..#.#..",
        /*09*/ ".#.........#..",
        /*10*/ ".###..@...##..",
        /*11*/ "....#.B.#.....",
        /*12*/ "..............."
    };

    static const char *LEVEL_3[MAP_ROWS] = {
        /*00*/ ".@.@.@.@.@.@.@.",
        /*01*/ "...............",
        /*02*/ ".@.###.###.@...",
        /*03*/ "..#.......#...",
        /*04*/ "..#..@.@..#...",
        /*05*/ "..#.......#...",
        /*06*/ ".@.###.###.@...",
        /*07*/ "...............",
        /*08*/ ".@.@.....@.@...",
        /*09*/ "..#...###..#..",
        /*10*/ "..#...#...#@..",
        /*11*/ "....#.B.#.....",
        /*12*/ "..............."
    };

    const char **selected = LEVEL_1;
    int enemyCount = 10;

    if (level == 1)      { selected = LEVEL_1; enemyCount = 10; }
    else if (level == 2) { selected = LEVEL_2; enemyCount = 15; }
    else                 { selected = LEVEL_3; enemyCount = 20; }

    // apply level data
    for (int r = 0; r < MAP_ROWS; ++r) {
        const char *row = selected[r];
        for (int c = 0; c < MAP_COLS && row[c] != '\0'; ++c) {
            switch (row[c]) {
            case '#': m_map[r][c] = CELL_BRICK; break;
            case '@': m_map[r][c] = CELL_STEEL; break;
            case 'B': m_map[r][c] = CELL_BASE;  break;
            default:  break;   // '.' → leave empty
            }
        }
    }

    // always place base
    m_map[11][7] = CELL_BASE;
    m_basePos    = QPointF(7 * CELL_SIZE, 11 * CELL_SIZE);

    m_enemiesToSpawn = enemyCount;
    m_enemiesKilled  = 0;
    m_spawnTimer     = 0;

    // reset player
    QPointF sp = playerSpawnPos();
    m_player.x             = sp.x();
    m_player.y             = sp.y();
    m_player.dir           = DIR_UP;
    m_player.alive         = true;
    m_player.shootCooldown = 0;
    m_invulnTimer          = 60;
}
