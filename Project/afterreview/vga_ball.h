#ifndef _VGA_BALL_H
#define _VGA_BALL_H

#include <linux/ioctl.h>

/* 定义最大对象数量 */
#define MAX_OBJECTS 20

/* Color structure */
typedef struct {
    unsigned char red, green, blue;
} vga_ball_color_t;

/* 新的游戏对象结构 */
typedef struct {
    unsigned short x;          // 12位x坐标
    unsigned short y;          // 12位y坐标
    unsigned char sprite_idx;  // 6位精灵索引
    unsigned char active;      // 活动状态
} vga_ball_object_t;

/* 主要参数结构体用于ioctl调用 */
typedef struct {
    vga_ball_color_t background;
    vga_ball_object_t objects[MAX_OBJECTS];
} vga_ball_arg_t;

/* 定义对象索引常量 */
#define SHIP_INDEX 0
#define ENEMY1_INDEX 1
#define ENEMY2_INDEX 2
#define BULLET_START_INDEX 3
#define ENEMY_BULLET_START_INDEX 8

/* 精灵索引常量 */
#define SHIP_SPRITE 0
#define ENEMY_SPRITE 1
#define BULLET_SPRITE 17
#define ENEMY_BULLET_SPRITE 18

#define VGA_BALL_MAGIC 'v'

/* ioctls和它们的参数 */
#define VGA_BALL_WRITE_BACKGROUND   _IOW(VGA_BALL_MAGIC, 1, vga_ball_arg_t)
#define VGA_BALL_READ_BACKGROUND    _IOR(VGA_BALL_MAGIC, 2, vga_ball_arg_t)
#define VGA_BALL_WRITE_OBJECTS      _IOW(VGA_BALL_MAGIC, 3, vga_ball_arg_t)
#define VGA_BALL_READ_OBJECTS       _IOR(VGA_BALL_MAGIC, 4, vga_ball_arg_t)
#define VGA_BALL_UPDATE_GAME_STATE  _IOW(VGA_BALL_MAGIC, 5, vga_ball_arg_t)
#define VGA_BALL_SET_RANDOM_BG      _IOW(VGA_BALL_MAGIC, 6, int)

#endif /* _VGA_BALL_H */