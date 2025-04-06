#ifndef _VGA_SPACESHIP_H
#define _VGA_SPACESHIP_H

#include <linux/ioctl.h>

/* Color structure */
typedef struct {
    unsigned char red, green, blue;
} vga_spaceship_color_t;

/* Position structure for spaceships and bullets */
typedef struct {
    unsigned short x;  // x coordinate (0-1279)
    unsigned short y;  // y coordinate (0-479)
} vga_spaceship_position_t;

/* 
 * Game object structure with position and status
 * active field is used for bullets to determine if they are currently flying
 */
typedef struct {
    vga_spaceship_position_t position;
    unsigned char active;      // For bullets: 1 = active, 0 = inactive
} vga_spaceship_object_t;

/* 
 * Main argument structure for ioctl calls
 * Contains all game state information
 */
typedef struct {
    vga_spaceship_color_t background;
    vga_spaceship_object_t ship1;
    vga_spaceship_object_t ship2;
    vga_spaceship_object_t bullet1;
    vga_spaceship_object_t bullet2;
} vga_spaceship_arg_t;

#define VGA_SPACESHIP_MAGIC 'v'

/* ioctls and their arguments */
#define VGA_SPACESHIP_WRITE_BACKGROUND   _IOW(VGA_SPACESHIP_MAGIC, 1, vga_spaceship_arg_t)
#define VGA_SPACESHIP_READ_BACKGROUND    _IOR(VGA_SPACESHIP_MAGIC, 2, vga_spaceship_arg_t)
#define VGA_SPACESHIP_WRITE_SHIP1        _IOW(VGA_SPACESHIP_MAGIC, 3, vga_spaceship_arg_t)
#define VGA_SPACESHIP_READ_SHIP1         _IOR(VGA_SPACESHIP_MAGIC, 4, vga_spaceship_arg_t)
#define VGA_SPACESHIP_WRITE_SHIP2        _IOW(VGA_SPACESHIP_MAGIC, 5, vga_spaceship_arg_t)
#define VGA_SPACESHIP_READ_SHIP2         _IOR(VGA_SPACESHIP_MAGIC, 6, vga_spaceship_arg_t)
#define VGA_SPACESHIP_WRITE_BULLET1      _IOW(VGA_SPACESHIP_MAGIC, 7, vga_spaceship_arg_t)
#define VGA_SPACESHIP_READ_BULLET1       _IOR(VGA_SPACESHIP_MAGIC, 8, vga_spaceship_arg_t)
#define VGA_SPACESHIP_WRITE_BULLET2      _IOW(VGA_SPACESHIP_MAGIC, 9, vga_spaceship_arg_t)
#define VGA_SPACESHIP_READ_BULLET2       _IOR(VGA_SPACESHIP_MAGIC, 10, vga_spaceship_arg_t)
#define VGA_SPACESHIP_UPDATE_GAME_STATE  _IOW(VGA_SPACESHIP_MAGIC, 11, vga_spaceship_arg_t)

#endif /* _VGA_SPACESHIP_H */