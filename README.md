# 坦克大战 - Tank Battle

A classic **Battle City (坦克大战)** clone built with **Qt 5.12.12** and C++11.

![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS-lightgrey)
![Qt](https://img.shields.io/badge/Qt-5.12.12-green)
![Language](https://img.shields.io/badge/C%2B%2B-11-blue)

## Gameplay

Destroy all enemy tanks while protecting your **base (Eagle)** at the bottom of the map.

- **3 levels** with increasing difficulty
- **3 enemy types**: Normal, Fast (blue), Heavy (purple — takes 2 hits)
- Brick walls can be destroyed; steel walls are indestructible
- Lose a life if hit; game over when all lives are lost
- **Game over immediately** if the base is destroyed!

## Controls

| Action        | Keys                    |
|---------------|-------------------------|
| Move          | `W` `A` `S` `D` or Arrow Keys |
| Fire          | `Space` or `J`          |
| Pause         | `P` or `Esc`            |
| Start / Retry | `Enter`                 |

## Building

### Prerequisites

- **Qt 5.12.12** (or any Qt 5.x)
- A C++11 compiler (MSVC 2017+, GCC 7+, Clang 5+)

### Build Steps

```bash
cd TankBattle
qmake TankBattle.pro
make          # or nmake / mingw32-make on Windows
```

Or open `TankBattle.pro` directly in **Qt Creator** and build.

## Project Structure

```
TankBattle/
├── TankBattle.pro      # Qt project file
├── main.cpp            # Entry point
├── gameengine.h/.cpp   # Core game logic (tanks, bullets, collision, AI)
├── gamewidget.h/.cpp   # Rendering and keyboard input
└── README.md
```

## Game Features

- **60 FPS** game loop with smooth pixel-based movement
- **AI enemies** with directional bias toward the player
- Bullet-to-bullet collision (they cancel each other)
- Brief invulnerability after respawn (flashing tank)
- HUD showing level, score, lives, and enemy count
- Pause / resume support
- Level progression with escalating enemy counts

## License

MIT — feel free to use, modify, and share!
