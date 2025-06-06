#ifndef _VGA_BALL_H
#define _VGA_BALL_H

#include <linux/ioctl.h>

/* 定义最大子弹数量 */
#define MAX_BULLETS 5
#define ENEMY_COUNT 130
#define LIFE_COUNT 5
// #define INPUT_QUEUE_SIZE 32


/* Color structure */
typedef struct {
    unsigned char red, green, blue;
} background_color;

typedef struct {
    unsigned short pos_x, pos_y;
    unsigned short velo_x, velo_y;
    int lives, num_bullets;
    char sprite;
    int active;
} spaceship;

typedef struct {
    unsigned short pos_x, pos_y;
    unsigned short velo_y; // velo_x is always 0 on bullets
    int active;
} bullet;

typedef struct {
    unsigned short pos_x, pos_y;
    // unsigned short velo_x, velo_y;
    // int moving; // if the enemy is currently moving towards the ship
    bullet bul; // bullet structure for each enemy to shoot
    char sprite; // different enemies have different visuals
    int active;
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

