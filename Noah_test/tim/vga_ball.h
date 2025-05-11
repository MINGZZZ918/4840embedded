#ifndef _VGA_BALL_H
#define _VGA_BALL_H

#include <linux/ioctl.h>
#include <stdbool.h>

/* 定义最大子弹数量 */
#define MAX_BULLETS 5
#define ENEMY_COUNT 50
#define LIFE_COUNT 5
#define NUM_ROWS 5

/* Color structure */
typedef struct {
    unsigned char red, green, blue;
} background_color;

typedef struct {
    unsigned short pos_x, pos_y;
    short velo_x, velo_y;
    char lives, num_bullets;
    bool active;
} spaceship;

typedef struct {
    unsigned short pos_x, pos_y;
    char velo_y; // velo_x is always 0 on bullets
    bool active;
} bullet;

typedef struct {
    unsigned short pos_x, pos_y;
    char velo_x, velo_y;
    bullet bul;
    char sprite;
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

