/*
 * Userspace program for the VGA Ball game
 * Demonstrates a spaceship automatically firing multiple bullets
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "vga_ball.h"

/* File descriptor for the VGA ball device */
int vga_ball_fd;

/* Screen dimensions */
#define SCREEN_WIDTH 1280//640?
#define SCREEN_HEIGHT 480

/* Ship constants */
#define SHIP_WIDTH 16  // 与vga_ball.sv中一致的像素艺术飞船宽度
#define SHIP_HEIGHT 16 // 与vga_ball.sv中一致的像素艺术飞船高度
#define SHIP_INITIAL_X 200
#define SHIP_INITIAL_Y 240
#define SHIP_VERTICAL_SPEED 1 // 飞船垂直移动速度
#define SHIP_MIN_Y 50         // 飞船Y坐标最小值，确保不超出屏幕上边界
#define SHIP_MAX_Y (SCREEN_HEIGHT - SHIP_HEIGHT - 50) // 飞船Y坐标最大值，确保不超出屏幕下边界

/* Bullet constants */
#define BULLET_SIZE 4
#define BULLET_SPEED 10

/* 敌人常量 */
#define ENEMY_WIDTH 16
#define ENEMY_HEIGHT 16
#define ENEMY1_INITIAL_X 800
#define ENEMY1_INITIAL_Y 150
#define ENEMY2_INITIAL_X 800
#define ENEMY2_INITIAL_Y 350
#define ENEMY_BULLET_SIZE 4
#define ENEMY_BULLET_SPEED 8
#define ENEMY_BULLET_COOLDOWN_FRAMES 60 // 约1秒发射一次

/* Animation and game timing */
#define FRAME_DELAY_MS 16  // ~60 FPS
#define BULLET_COOLDOWN_FRAMES 20 // 0.33 second between shots

/* Game state */
vga_ball_arg_t game_state;
int bullet_cooldown = 0;
int direction = 1; // 垂直移动方向: 1=向下, -1=向上

/* Array of background colors to cycle through */
static const vga_ball_color_t colors[] = {
    { 0x00, 0x00, 0x10 },  // Very dark blue
    { 0x00, 0x00, 0x20 },  // Dark blue
    { 0x10, 0x10, 0x30 },  // Navy blue
    { 0x00, 0x00, 0x40 },  // Medium blue
    { 0x20, 0x20, 0x40 }   // Blue-purple
};
#define COLOR_COUNT 5

/**
 * Initialize game state
 */
void init_game_state(void) {
    int i;
    
    // 设置背景色
    game_state.background.red = 0x00;
    game_state.background.green = 0x00;
    game_state.background.blue = 0x20;  // 深蓝色背景
    
    // 飞船(左侧)
    game_state.ship.position.x = SHIP_INITIAL_X;
    game_state.ship.position.y = SHIP_INITIAL_Y;
    game_state.ship.active = 1;
    
    // 初始化所有玩家子弹为非激活状态
    for (i = 0; i < MAX_BULLETS; i++) {
        game_state.bullets[i].position.x = 0;
        game_state.bullets[i].position.y = 0;
        game_state.bullets[i].active = 0;
    }
    
    // 初始化敌人
    game_state.enemies[0].position.x = ENEMY1_INITIAL_X;
    game_state.enemies[0].position.y = ENEMY1_INITIAL_Y;
    game_state.enemies[0].active = 1;
    
    game_state.enemies[1].position.x = ENEMY2_INITIAL_X;
    game_state.enemies[1].position.y = ENEMY2_INITIAL_Y;
    game_state.enemies[1].active = 1;
    
    // 初始化所有敌人子弹为非激活状态
    for (i = 0; i < MAX_ENEMY_BULLETS; i++) {
        game_state.enemy_bullets[i].position.x = 0;
        game_state.enemy_bullets[i].position.y = 0;
        game_state.enemy_bullets[i].active = 0;
    }
}

/**
 * Update game state and send to the device
 */
void update_hardware(void) {
    if (ioctl(vga_ball_fd, VGA_BALL_UPDATE_GAME_STATE, &game_state)) {
        perror("ioctl(VGA_BALL_UPDATE_GAME_STATE) failed");
        exit(EXIT_FAILURE);
    }
}

/**
 * Find an inactive bullet slot and fire a new bullet
 */
/**
 * Find an inactive bullet slot and fire a new bullet
 */
void fire_bullet(void) {
    int i;
    
    if (bullet_cooldown <= 0) {
        // 寻找非活动的子弹槽位
        for (i = 0; i < MAX_BULLETS; i++) {
            if (!game_state.bullets[i].active) {
                // 设置子弹位置为飞船前方
                game_state.bullets[i].position.x = game_state.ship.position.x + SHIP_WIDTH;
                game_state.bullets[i].position.y = game_state.ship.position.y + (SHIP_HEIGHT / 2) - (BULLET_SIZE / 2);
                game_state.bullets[i].active = 1;
                
                // 重置冷却时间
                bullet_cooldown = BULLET_COOLDOWN_FRAMES;
                printf("发射子弹 %d: 位置 x=%d, y=%d\n", 
                       i, 
                       game_state.bullets[i].position.x,
                       game_state.bullets[i].position.y);
                break;
            }
        }
    }
}

/**
 * Update all bullet positions and check for out-of-bounds
 */
void update_bullets(void) {
    int i;
    
    // 更新所有激活的子弹
    for (i = 0; i < MAX_BULLETS; i++) {
        if (game_state.bullets[i].active) {
            // 移动子弹
            game_state.bullets[i].position.x += BULLET_SPEED;
            
            // 检查是否超出屏幕，确保子弹在超出屏幕后设置为非活动状态
            if (game_state.bullets[i].position.x >= SCREEN_WIDTH - BULLET_SIZE) {
                // 先将子弹设置为非活动状态
                game_state.bullets[i].active = 0;
                
                // 然后重置位置到屏幕外，防止出现回卷
                game_state.bullets[i].position.x = 0;
                game_state.bullets[i].position.y = 0;
                
                printf("子弹 %d 消失: 位置 x=%d\n", i, game_state.bullets[i].position.x);
            }
        }
    }
}

/**
 * 敌人随机发射子弹函数
 */
void enemy_fire_bullet(int enemy_index) {
    int i;
    static int enemy_cooldowns[2] = {0, 0}; // 每个敌人独立的冷却时间
    
    // 检查敌人是否活动
    if (!game_state.enemies[enemy_index].active) {
        return;
    }
    
    // 检查冷却时间
    if (enemy_cooldowns[enemy_index] <= 0) {
        // 随机决定是否发射子弹 (约20%概率)
        if (rand() % 100 < 20) {
            // 计算子弹槽范围 (每个敌人最多3颗子弹)
            int start_slot = enemy_index * 3;
            int end_slot = start_slot + 3;
            
            // 寻找非活动的子弹槽位
            for (i = start_slot; i < end_slot; i++) {
                if (!game_state.enemy_bullets[i].active) {
                    // 设置子弹位置为敌人前方
                    game_state.enemy_bullets[i].position.x = game_state.enemies[enemy_index].position.x - ENEMY_BULLET_SIZE;
                    game_state.enemy_bullets[i].position.y = game_state.enemies[enemy_index].position.y + (ENEMY_HEIGHT / 2);
                    game_state.enemy_bullets[i].active = 1;
                    
                    // 重置冷却时间 (随机化冷却时间，使敌人不同步发射)
                    enemy_cooldowns[enemy_index] = ENEMY_BULLET_COOLDOWN_FRAMES + (rand() % 60);
                    
                    printf("敌人 %d 发射子弹: 位置 x=%d, y=%d\n", 
                           enemy_index, 
                           game_state.enemy_bullets[i].position.x,
                           game_state.enemy_bullets[i].position.y);
                    break;
                }
            }
        } else {
            // 即使不发射，也设置一个较短的冷却时间
            enemy_cooldowns[enemy_index] = 15 + (rand() % 30);
        }
    } else {
        // 递减冷却时间
        enemy_cooldowns[enemy_index]--;
    }
}

/**
 * 更新敌人子弹位置函数
 */
void update_enemy_bullets(void) {
    int i;
    
    // 更新所有激活的敌人子弹
    for (i = 0; i < MAX_ENEMY_BULLETS; i++) {
        if (game_state.enemy_bullets[i].active) {
            // 敌人子弹向左移动
            game_state.enemy_bullets[i].position.x -= ENEMY_BULLET_SPEED;
            
            // 检查是否超出屏幕左侧
            if (game_state.enemy_bullets[i].position.x <= 0) {
                // 将子弹设置为非活动状态
                game_state.enemy_bullets[i].active = 0;
                
                // 重置位置到屏幕外
                game_state.enemy_bullets[i].position.x = 0;
                game_state.enemy_bullets[i].position.y = 0;
                
                printf("敌人子弹 %d 消失: 越过左边界\n", i);
            }
        }
    }
}

/**
 * Main function - runs the game loop
 */
int main(void) {
    static const char filename[] = "/dev/vga_ball";
    int frame_count = 0;
    int color_index = 0;

    printf("VGA Ball Demo started\n");

    /* Open the device file */
    if ((vga_ball_fd = open(filename, O_RDWR)) == -1) {
        fprintf(stderr, "Could not open %s\n", filename);
        return EXIT_FAILURE;
    }

    /* Initialize random number generator */
    srand(time(NULL));
    
    /* Initialize game state */
    init_game_state();
    update_hardware();
    
    printf("Starting animation, press Ctrl+C to exit...\n");
    printf("添加了两个敌人，位置: (%d,%d) 和 (%d,%d)\n", 
           ENEMY1_INITIAL_X, ENEMY1_INITIAL_Y, 
           ENEMY2_INITIAL_X, ENEMY2_INITIAL_Y);
    
    /* Main game loop */
    while (1) {
        /* Decrement cooldown */
        if (bullet_cooldown > 0) bullet_cooldown--;
        
        /* Periodically change background color */
        if (frame_count % 120 == 0) {
            color_index = (color_index + 1) % COLOR_COUNT;
            game_state.background = colors[color_index];
        }
        
        /* Always try to fire bullet when cooldown allows */
        fire_bullet();
        
        /* 敌人尝试发射子弹 */
        enemy_fire_bullet(0);  // 敌人1
        enemy_fire_bullet(1);  // 敌人2
        
        /* Move bullets */
        update_bullets();
        
        /* 更新敌人子弹 */
        update_enemy_bullets();
        
        /* 垂直方向飞船移动 - 确保平滑移动 */
        if (direction > 0) {
            game_state.ship.position.y += SHIP_VERTICAL_SPEED;
            if (game_state.ship.position.y >= SHIP_MAX_Y) {
                direction = -1;
                printf("飞船改变方向: 向上\n");
            }
        } else {
            game_state.ship.position.y -= SHIP_VERTICAL_SPEED;
            if (game_state.ship.position.y <= SHIP_MIN_Y) {
                direction = 1;
                printf("飞船改变方向: 向下\n");
            }
        }
        
        /* Update the hardware */
        update_hardware();
        
        /* Print debug information */
        if (frame_count % 60 == 0) { // 每秒显示一次状态
            printf("飞船位置: x=%d, y=%d\n", 
                   game_state.ship.position.x, 
                   game_state.ship.position.y);
            
            // 打印活动子弹数量
            int active_count = 0;
            for (int i = 0; i < MAX_BULLETS; i++) {
                if (game_state.bullets[i].active) {
                    active_count++;
                }
            }
            printf("玩家活动子弹数量: %d\n", active_count);
            
            // 打印敌人子弹数量
            int enemy_bullet_count = 0;
            for (int i = 0; i < MAX_ENEMY_BULLETS; i++) {
                if (game_state.enemy_bullets[i].active) {
                    enemy_bullet_count++;
                }
            }
            printf("敌人活动子弹数量: %d\n", enemy_bullet_count);
        }
        
        /* Delay for next frame */
        usleep(FRAME_DELAY_MS * 1000);
        
        /* Increment frame counter */
        frame_count++;
    }
    
    return 0;
}