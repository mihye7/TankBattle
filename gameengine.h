#ifndef GAMEENGINE_H
#define GAMEENGINE_H

#include <QObject>
#include <QTimer>
#include <QList>
#include <QSet>
#include <QRectF>
#include <QPointF>

// ============================================================
// Game Constants
// ============================================================
constexpr int CELL_SIZE       = 40;
constexpr int MAP_COLS        = 15;
constexpr int MAP_ROWS        = 13;
constexpr int GAME_WIDTH      = MAP_COLS * CELL_SIZE;   // 600
constexpr int GAME_HEIGHT     = MAP_ROWS * CELL_SIZE;   // 520
constexpr int TANK_SIZE       = 34;
constexpr int BULLET_SIZE     = 6;
constexpr int PLAYER_SPEED    = 4;
constexpr int ENEMY_SPEED     = 2;
constexpr int BULLET_SPEED    = 8;
constexpr int MAX_ON_SCREEN   = 4;
constexpr int PLAYER_LIVES    = 3;
constexpr int INVULN_FRAMES   = 150;
constexpr int SHOOT_COOLDOWN  = 18;
constexpr int ENEMY_COOLDOWN  = 50;

// ============================================================
// Enums
// ============================================================
enum Direction {
    DIR_UP    = 0,
    DIR_DOWN  = 1,
    DIR_LEFT  = 2,
    DIR_RIGHT = 3
};

enum CellType {
    CELL_EMPTY = 0,
    CELL_BRICK = 1,
    CELL_STEEL = 2,
    CELL_BASE  = 3
};

enum GameState {
    STATE_MENU,
    STATE_PLAYING,
    STATE_WIN,
    STATE_LOSE
};

enum EnemyType {
    ENEMY_NORMAL = 0,
    ENEMY_FAST   = 1,
    ENEMY_HEAVY  = 2
};

// ============================================================
// Data Structures
// ============================================================
struct Tank {
    qreal x, y;
    Direction dir;
    int speed;
    int health;
    int maxHealth;
    bool alive;
    int shootCooldown;
    int moveTimer;
    bool isPlayer;
    EnemyType enemyType;
};

struct Bullet {
    qreal x, y;
    Direction dir;
    int speed;
    bool active;
};

// ============================================================
// GameEngine — core logic
// ============================================================
class GameEngine : public QObject
{
    Q_OBJECT

public:
    explicit GameEngine(QObject *parent = nullptr);

    void startGame();
    void resetGame();
    void togglePause();

    // ---- state accessors ----
    GameState state()      const { return m_state; }
    int       score()      const { return m_score; }
    int       lives()      const { return m_lives; }
    int       level()      const { return m_level; }
    int       enemiesKilled()  const { return m_enemiesKilled; }
    int       enemiesToSpawn() const { return m_enemiesToSpawn; }
    int       invulnTimer()    const { return m_invulnTimer; }
    bool      isFrozen()       const { return m_freezeTimer > 0; }

    // ---- map ----
    int cellType(int row, int col) const { return m_map[row][col]; }

    // ---- entities (const refs for the painter) ----
    const Tank&            player()        const { return m_player; }
    const QList<Tank>&     enemies()       const { return m_enemies; }
    const QList<Bullet>&   playerBullets() const { return m_playerBullets; }
    const QList<Bullet>&   enemyBullets()  const { return m_enemyBullets; }
    QPointF                basePos()       const { return m_basePos; }

signals:
    void updated();

public slots:
    void keyPress(int key);
    void keyRelease(int key);

private slots:
    void tick();

private:
    QTimer *m_timer;

    // state
    GameState m_state;
    int m_score;
    int m_lives;
    int m_level;
    int m_invulnTimer;
    int m_freezeTimer;

    // map
    int m_map[MAP_ROWS][MAP_COLS];
    QPointF m_basePos;

    // entities
    Tank m_player;
    QList<Tank>   m_enemies;
    QList<Bullet> m_playerBullets;
    QList<Bullet> m_enemyBullets;

    // spawn control
    int m_enemiesToSpawn;
    int m_enemiesKilled;
    int m_spawnTimer;
    QList<QPointF> m_spawnPoints;

    // input
    QSet<int> m_keysHeld;

    // ---- private helpers ----
    void  loadLevel(int level);
    void  updatePlayer();
    void  updateEnemies();
    void  updateBullets(QList<Bullet> &bullets);
    void  checkCollisions();
    void  spawnEnemy();
    void  playerShoot();
    void  enemyShoot(Tank &enemy);
    void  destroyPlayer();

    QRectF tankRect(qreal x, qreal y)  const;
    QRectF tankRect(const Tank &t)     const { return tankRect(t.x, t.y); }
    QRectF bulletRect(const Bullet &b) const;
    bool   canMoveTo(const QRectF &rect, const Tank *exclude = nullptr) const;
    bool   checkWallCollision(const QRectF &rect, const Tank *exclude = nullptr) const;
    QPointF playerSpawnPos() const;
};

#endif // GAMEENGINE_H
