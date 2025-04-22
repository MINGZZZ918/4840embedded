#ifndef _VGA_BALL_H
#define _VGA_BALL_H

#include "vga_ball.h"
#include <linux/ioctl.h>

/* 定义最大子弹数量 */
#define MAX_BULLETS 5
#define MAX_ENEMY_BULLETS 6  // 每个敌人3颗子弹

/* Color structure */
typedef struct {
    unsigned char red, green, blue;
} background_color;

typedef struct {
    unsigned short pos_x, pos_y;
    int velo_x, velo_y, lives, num_bullets;
    unsigned char active;
} spaceship;

typedef struct {
    unsigned short pos_x, pos_y;
    unsigned short velo_y; // velo_x is always 0 on bullets
    int active;
} bullet;

typedef struct {
    background_color background;
    spaceship ship;
    bullet bullets[MAX_BULLETS];
} gamestate;

#define VGA_BALL_MAGIC 'v'

/* ioctls and their arguments */
#define VGA_BALL_UPDATE_GAME_STATE  _IOW(VGA_BALL_MAGIC, 1, gamestate)


#endif /* _VGA_BALL_H */