#ifndef _VGA_BALL_H
#define _VGA_BALL_H

#include <linux/ioctl.h>
#include <stdbool.h>

/* 定义最大子弹数量 */
#define MAX_BULLETS 5
#define ENEMY_COUNT 22
#define LIFE_COUNT 5

/* Color structure */
typedef struct {
    unsigned char red, green, blue;
} background_color;

typedef struct {
    unsigned short pos_x, pos_y;
    short velo_x, velo_y;
    short lives, num_bullets;
    bool active;
} spaceship;

typedef struct {
    unsigned short pos_x, pos_y;
    short velo_x, velo_y;
    bool active;
} bullet;

typedef struct {
    unsigned short pos_x, pos_y;
    short velo_x, velo_y;
    short start_x, start_y;
    bullet bul;
    short move_time, bul_cooldown;
    char sprite, turn_counter, row, col;
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
#define VGA_BALL_UPDATE_GAME_STATE   _IOW(VGA_BALL_MAGIC, 1, gamestate)


#endif /* _VGA_BALL_H */

