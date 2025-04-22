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

/* Position structure for ship and bullet */
typedef struct {
    unsigned short x;  // x coordinate (0-1279)
    unsigned short y;  // y coordinate (0-479)
} vga_ball_position_t;

/* 
 * Game object structure with position and status
 * active field is used for bullet to determine if it is currently flying
 */
typedef struct {
    unsigned short x, y;
    int velo_x, velo_y, lives, num_bullets;
    unsigned char active;
} spaceship;

/* 
 * Main argument structure for ioctl calls
 * Contains all game state information
 */
typedef struct {
    background_color background;
    spaceship ship;
} vga_ball_arg_t;

#define VGA_BALL_MAGIC 'v'

/* ioctls and their arguments */
#define VGA_BALL_UPDATE_GAME_STATE  _IOW(VGA_BALL_MAGIC, 1, vga_ball_arg_t)


#endif /* _VGA_BALL_H */