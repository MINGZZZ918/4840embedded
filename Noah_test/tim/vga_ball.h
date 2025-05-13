#ifndef _VGA_BALL_H
#define _VGA_BALL_H

#include <linux/ioctl.h>
#include <stdbool.h>

/* 定义最大子弹数量 */
#define SHIP_BULLETS 5
#define MAX_BULLETS 10
#define ENEMY_COUNT 60
#define LIFE_COUNT 5

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
    short velo_x, velo_y;
    short lives;
    bullet bullets[SHIP_BULLETS];
    bool active;
} spaceship;

typedef struct {
    unsigned short pos_x, pos_y;
    short velo_x, velo_y;
    short start_x, start_y;

    short move_time, turn_counter, bul_cooldown, bul;
    char sprite, row, col, position;
    bool active, returning, moving;
} enemy;


typedef struct {
    spaceship ship;
    bullet bullets[MAX_BULLETS];
    enemy enemies[ENEMY_COUNT];
    background_color background;
} gamestate;

#define VGA_BALL_MAGIC 'v'

/* ioctls and their arguments */
#define VGA_BALL_UPDATE_ENEMIES   _IOW(VGA_BALL_MAGIC, 1, gamestate)
#define VGA_BALL_UPDATE_SHIP   _IOW(VGA_BALL_MAGIC, 2, spaceship)
#define VGA_BALL_UPDATE_SHIP_BULLETS   _IOW(VGA_BALL_MAGIC, 3, spaceship)


#endif /* _VGA_BALL_H */

