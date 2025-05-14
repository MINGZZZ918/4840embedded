#ifndef _VGA_BALL_H
#define _VGA_BALL_H

#include <linux/ioctl.h>
#include <stdbool.h>

/* 定义最大子弹数量 */
#define SHIP_BULLETS 5
#define MAX_BULLETS 10
#define ENEMY_COUNT 60

#define SHIP_WIDTH 16
#define SHIP_HEIGHT 16

#define SHIP 0
#define SHIP_LEFT 1
#define SHIP_RIGHT 2

#define SHIP_BULLET 3

#define ENEMY1 2
#define ENEMY2 3
#define ENEMY3 4

#define ENEMY_BULLET 7
#define ENEMY_BULLET_LEFT 8
#define ENEMY_BULLET_RIGHT 9

#define EXTRA_LIFE 2
#define SHIP_SPEED 3
#define EXTRA_BULLETS 4

#define SHIP_FLAME 13
#define SHIP_EXPLOSION1 0
#define SHIP_EXPLOSION2 1

/* Color structure */
typedef struct {
    unsigned char red, green, blue;
} background_color;

typedef struct {
    unsigned short pos_x, pos_y;
    short velo_x, velo_y;
    short enemy;
    bool active;
} bullet;

typedef struct {
    unsigned short pos_x, pos_y;
    char sprite;
    bool active;
} powerup;

typedef struct {
    unsigned short pos_x, pos_y;
    short velo_x, velo_y;
    short lives, num_buls;
    bullet bullets[SHIP_BULLETS];
    bool active;
} spaceship;

typedef struct {
    unsigned short pos_x, pos_y;
    short velo_x, velo_y;
    short start_x, start_y;

    short move_time, turn_counter, bul_cooldown, bul, explosion_timer;
    char sprite, row, col, position;
    bool active, returning, moving;
} enemy;


typedef struct {
    spaceship ship;
    bullet bullets[MAX_BULLETS];
    enemy enemies[ENEMY_COUNT];
    background_color background;
    powerup power_up;
} gamestate;

#define VGA_BALL_MAGIC 'v'

/* ioctls and their arguments */
#define UPDATE_ENEMIES   _IOW(VGA_BALL_MAGIC, 1, gamestate)
#define UPDATE_SHIP   _IOW(VGA_BALL_MAGIC, 2, spaceship)
#define UPDATE_SHIP_BULLETS   _IOW(VGA_BALL_MAGIC, 3, spaceship)
#define UPDATE_POWERUP   _IOW(VGA_BALL_MAGIC, 4, powerup)

#endif /* _VGA_BALL_H */

