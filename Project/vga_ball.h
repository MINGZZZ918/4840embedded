#ifndef _VGA_BALL_H
#define _VGA_BALL_H

#include <linux/ioctl.h>

/* 定义最大子弹数量 */
#define MAX_BULLETS 5

/* Color structure */
typedef struct {
    unsigned char red, green, blue;
} vga_ball_color_t;

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
    vga_ball_position_t position;
    unsigned char active;      // For bullet: 1 = active, 0 = inactive
} vga_ball_object_t;

/* 
 * Main argument structure for ioctl calls
 * Contains all game state information
 */
typedef struct {
    vga_ball_color_t background;
    vga_ball_object_t ship;
    vga_ball_object_t bullets[MAX_BULLETS]; // 现在支持多个子弹
} vga_ball_arg_t;

#define VGA_BALL_MAGIC 'v'

/* ioctls and their arguments */
#define VGA_BALL_WRITE_BACKGROUND   _IOW(VGA_BALL_MAGIC, 1, vga_ball_arg_t)
#define VGA_BALL_READ_BACKGROUND    _IOR(VGA_BALL_MAGIC, 2, vga_ball_arg_t)
#define VGA_BALL_WRITE_SHIP         _IOW(VGA_BALL_MAGIC, 3, vga_ball_arg_t)
#define VGA_BALL_READ_SHIP          _IOR(VGA_BALL_MAGIC, 4, vga_ball_arg_t)
#define VGA_BALL_WRITE_BULLETS      _IOW(VGA_BALL_MAGIC, 5, vga_ball_arg_t)
#define VGA_BALL_READ_BULLETS       _IOR(VGA_BALL_MAGIC, 6, vga_ball_arg_t)
#define VGA_BALL_UPDATE_GAME_STATE  _IOW(VGA_BALL_MAGIC, 7, vga_ball_arg_t)

#endif /* _VGA_BALL_H */