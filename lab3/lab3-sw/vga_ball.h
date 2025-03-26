#ifndef _VGA_BALL_H
#define _VGA_BALL_H

#include <linux/ioctl.h>

typedef struct {
  unsigned char red, green, blue;
} vga_ball_color_t;

typedef struct {
  short x,y;
  unsigned char radius;
} vga_ball_circle_t;

typedef struct {
  short dx,dy;
  short next_bound;
  short corner;
  float vx,vy,fx,fy;
  float speed;
} vga_ball_dir_t;


/*
dx/dy: Integer directional step; represents the discrete direction of motion per frame.

vx/vy: Floating-point velocity vector (normalized direction multiplied by speed); used for precise movement.

fx/fy: Fractional components used for subpixel accuracy; accumulate the residual of each movement step to simulate smooth motion.
fx/fy are used to accumulate the fractional part of the velocity over time, enabling subpixel-accurate movement. Once the accumulated value exceeds 1.0, an actual pixel step (dx/dy) is performed, and the remainder is preserved for future frames.

speed: Scalar value representing the overall speed of the ball.

next_bound: Indicates the next boundary the ball is expected to collide with (left, right, top, or bottom).

corner: Boolean flag indicating whether the ball is hitting a corner (i.e., both horizontal and vertical boundaries simultaneously), requiring a reflection in both directions.
 */


typedef struct {
  vga_ball_color_t c_color;
  vga_ball_color_t bg_color;
  vga_ball_circle_t circle;
} vga_ball_arg_t;



#define VGA_BALL_MAGIC 'q'

/* ioctls and their arguments */
#define VGA_BALL_WRITE_BACKGROUND _IOW(VGA_BALL_MAGIC, 1, vga_ball_arg_t)
#define VGA_BALL_READ_BACKGROUND  _IOR(VGA_BALL_MAGIC, 2, vga_ball_arg_t)

#endif
