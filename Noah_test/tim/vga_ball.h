#ifndef _VGA_BALL_H
#define _VGA_BALL_H

#include <linux/ioctl.h>
#include <stdbool.h>

/* 定义最大子弹数量 */
#define MAX_BULLETS 5
#define ENEMY_COUNT 50
#define LIFE_COUNT 5
// #define INPUT_QUEUE_SIZE 32


/* Color structure */
typedef struct {
    unsigned char red, green, blue;
} background_color;

// typedef enum {
//     LR_NONE,
//     LEFT,
//     RIGHT,

//     UD_NONE,
//     UP,
//     DOWN,

//     BUMPER_NONE,
//     BUMPER_L,
//     BUMPER_R,
//     BUMPER_LR,
//     START,
//     SELECT,

//     BUTTON_NONE,
//     BUTTON_A,
//     BUTTON_B,
//     BUTTON_X,
//     BUTTON_Y, 

//     QUEUE_EMPTY
// } event;

// #ifndef __KERNEL__
// typedef struct {
//     event events[INPUT_QUEUE_SIZE]; // holds the events to handle before next frame
//     int head; // current position in the queue 
//     int tail;
//     pthread_mutex_t lock; 
// } input_queue;
// #endif

typedef struct {
    unsigned short pos_x, pos_y;
    unsigned short velo_x, velo_y;
    int lives, num_bullets;
    char sprite;
    bool active;
} spaceship;

typedef struct {
    unsigned short pos_x, pos_y;
    unsigned short velo_y; // velo_x is always 0 on bullets
    char sprite;
    bool active;
} bullet;

typedef struct {
    unsigned short pos_x, pos_y;
    unsigned short velo_x, velo_y;
    bullet bul; // bullet structure for each enemy to shoot
    char sprite; // different enemies have different visuals
    bool active, returning;
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

