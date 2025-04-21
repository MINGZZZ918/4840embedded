#ifndef _VGA_BALL_H
#define _VGA_BALL_H

#include <linux/ioctl.h>

/* 定义最大子弹数量 */
#define MAX_BULLETS 5
#define ENEMY_COUNT 39
#define LIFE_COUNT 5
#define INPUT_QUEUE_SIZE 32


/* Color structure */
typedef struct {
    unsigned char red, green, blue;
} background_color;

typedef enum {
    LR_NONE,
    LEFT,
    RIGHT,

    UD_NONE,
    UP,
    DOWN,

    BUMPER_NONE,
    BUMPER_L,
    BUMPER_R,
    BUMPER_LR,
    START,
    SELECT,

    BUTTON_NONE,
    BUTTON_A,
    BUTTON_B,
    BUTTON_X,
    BUTTON_Y, 

    QUEUE_EMPTY
} event;

#ifndef __KERNEL__
typedef struct {
    event events[INPUT_QUEUE_SIZE]; // holds the events to handle before next frame
    int head; // current position in the queue 
    int tail;
    pthread_mutex_t lock; 
} input_queue;
#endif

typedef struct {
    unsigned short pos_x, pos_y;
    unsigned short velo_x, velo_y;
    int lives, num_bullets;
} spaceship;

typedef struct {
    unsigned short pos_x, pos_y;
    unsigned short velo_y; // velo_x is always 0 on bullets
    int active;
} bullet;

typedef struct {
    unsigned short pos_x, pos_y, start_x, start_y;
    unsigned short velo_x, velo_y;
    int sprite; // different enemies have different visuals
    int moving; // if the enemy is currently moving towards the ship
    bullet bul; // bullet structure for each enemy to shoot
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
#define VGA_BALL_WRITE_BACKGROUND    _IOW(VGA_BALL_MAGIC, 1, gamestate)
#define VGA_BALL_READ_BACKGROUND     _IOR(VGA_BALL_MAGIC, 2, gamestate)
#define VGA_BALL_WRITE_SHIP          _IOW(VGA_BALL_MAGIC, 3, gamestate)
#define VGA_BALL_READ_SHIP           _IOR(VGA_BALL_MAGIC, 4, gamestate)
#define VGA_BALL_WRITE_BULLETS       _IOW(VGA_BALL_MAGIC, 5, gamestate)
#define VGA_BALL_READ_BULLETS        _IOR(VGA_BALL_MAGIC, 6, gamestate)
#define VGA_BALL_WRITE_ENEMIES       _IOW(VGA_BALL_MAGIC, 7, gamestate)
#define VGA_BALL_READ_ENEMIES        _IOR(VGA_BALL_MAGIC, 8, gamestate)
#define VGA_BALL_UPDATE_GAME_STATE   _IOW(VGA_BALL_MAGIC, 9, gamestate)
#define VGA_BALL_WRITE_ENEMY_BULLETS _IOW(VGA_BALL_MAGIC, 10, gamestate)
#define VGA_BALL_READ_ENEMY_BULLETS  _IOR(VGA_BALL_MAGIC, 11, gamestate)


#endif /* _VGA_BALL_H */