/*
 * Userspace program for the VGA Ball game
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
#include "controller.h"

/* File descriptor for the VGA ball device */
int vga_ball_fd;

/* Screen dimensions */
#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480

/* Ship constants */
#define SHIP_WIDTH 16  // 所有精灵统一尺寸
#define SHIP_HEIGHT 16
#define SHIP_INITIAL_X 200
#define SHIP_INITIAL_Y 240
#define SHIP_VERTICAL_SPEED 4
#define SHIP_MIN_Y 50
#define SHIP_MAX_Y (SCREEN_HEIGHT - SHIP_HEIGHT - 50)

/* Bullet constants */
#define BULLET_SPEED 10
#define BULLET_COOLDOWN_FRAMES 20

/* Enemy constants */
#define ENEMY1_INITIAL_X 500
#define ENEMY1_INITIAL_Y 150
#define ENEMY2_INITIAL_X 500
#define ENEMY2_INITIAL_Y 350
#define ENEMY_BULLET_SPEED 8
#define ENEMY_BULLET_COOLDOWN_FRAMES 60

/* Animation and game timing */
#define FRAME_DELAY_MS 16  // ~60 FPS

/* Game state */
vga_ball_arg_t game_state;
int bullet_cooldown = 0;
int enemy_cooldowns[2] = {0, 0};

/* LFSR随机背景控制 */
int use_random_bg = 0;  // 默认不使用随机背景
int last_random_bg_toggle = 0; // 上次切换随机背景的时间

/* 控制器变量 */
struct libusb_device_handle *controller;
uint8_t endpoint_address;
controller_packet controller_input;
int transferred;

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
    
    // 初始化所有对象为不活动
    for (i = 0; i < MAX_OBJECTS; i++) {
        game_state.objects[i].x = 0;
        game_state.objects[i].y = 0;
        game_state.objects[i].sprite_idx = 0;
        game_state.objects[i].active = 0;
    }
    
    // 飞船
    game_state.objects[SHIP_INDEX].x = SHIP_INITIAL_X;
    game_state.objects[SHIP_INDEX].y = SHIP_INITIAL_Y;
    game_state.objects[SHIP_INDEX].sprite_idx = SHIP_SPRITE;
    game_state.objects[SHIP_INDEX].active = 1;
    
    // 敌人1
    game_state.objects[ENEMY1_INDEX].x = ENEMY1_INITIAL_X;
    game_state.objects[ENEMY1_INDEX].y = ENEMY1_INITIAL_Y;
    game_state.objects[ENEMY1_INDEX].sprite_idx = ENEMY_SPRITE;
    game_state.objects[ENEMY1_INDEX].active = 1;
    
    // 敌人2
    game_state.objects[ENEMY2_INDEX].x = ENEMY2_INITIAL_X;
    game_state.objects[ENEMY2_INDEX].y = ENEMY2_INITIAL_Y;
    game_state.objects[ENEMY2_INDEX].sprite_idx = ENEMY_SPRITE;
    game_state.objects[ENEMY2_INDEX].active = 1;
    
    // 初始化玩家子弹
    for (i = BULLET_START_INDEX; i < ENEMY_BULLET_START_INDEX; i++) {
        game_state.objects[i].sprite_idx = BULLET_SPRITE;
    }
    
    // 初始化敌人子弹
    for (i = ENEMY_BULLET_START_INDEX; i < MAX_OBJECTS; i++) {
        game_state.objects[i].sprite_idx = ENEMY_BULLET_SPRITE;
    }
}

/**
 * 设置随机背景模式
 */
void set_random_background_mode(int enable) {
    use_random_bg = enable;
    
    if (ioctl(vga_ball_fd, VGA_BALL_SET_RANDOM_BG, &use_random_bg)) {
        perror("ioctl(VGA_BALL_SET_RANDOM_BG) failed");
        return;
    }
    
    printf("随机背景模式: %s\n", use_random_bg ? "开启" : "关闭");
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

void fire_bullet(void) {
    int i;
    
    if (bullet_cooldown <= 0) {
        // 寻找非活动的子弹槽位
        for (i = BULLET_START_INDEX; i < ENEMY_BULLET_START_INDEX; i++) {
            if (!game_state.objects[i].active) {
                // 设置子弹位置为飞船前方
                game_state.objects[i].x = game_state.objects[SHIP_INDEX].x + SHIP_WIDTH;
                game_state.objects[i].y = game_state.objects[SHIP_INDEX].y + (SHIP_HEIGHT / 2) - (SHIP_HEIGHT / 2);
                game_state.objects[i].active = 1;
                
                // 重置冷却时间
                bullet_cooldown = BULLET_COOLDOWN_FRAMES;
                printf("发射子弹 %d: 位置 x=%d, y=%d\n", 
                       i, 
                       game_state.objects[i].x,
                       game_state.objects[i].y);
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
    
    // 更新所有激活的玩家子弹
    for (i = BULLET_START_INDEX; i < ENEMY_BULLET_START_INDEX; i++) {
        if (game_state.objects[i].active) {
            // 移动子弹
            game_state.objects[i].x += BULLET_SPEED;
            
            // 检查是否超出屏幕
            if (game_state.objects[i].x >= SCREEN_WIDTH) {
                // 将子弹设置为非活动状态
                game_state.objects[i].active = 0;
                // 移动到屏幕外
                game_state.objects[i].x = 0;
                game_state.objects[i].y = 0;
                
                printf("子弹 %d 消失: 位置 x=%d\n", i, game_state.objects[i].x);
            }
        }
    }
}

/**
 * 敌人随机发射子弹函数
 */
void enemy_fire_bullet(int enemy_index) {
    int i;
    int enemy_obj_index = enemy_index + 1; // 敌人对象下标 (1和2)
    
    // 检查敌人是否活动
    if (!game_state.objects[enemy_obj_index].active) {
        return;
    }
    
    // 检查冷却时间
    if (enemy_cooldowns[enemy_index] <= 0) {
        // 随机决定是否发射子弹 (约20%概率)
        if (rand() % 100 < 20) {
            // 计算子弹槽范围 (每个敌人可发射3颗子弹)
            int start_slot = ENEMY_BULLET_START_INDEX + enemy_index * 3;
            int end_slot = start_slot + 3;
            
            // 寻找非活动的子弹槽位
            for (i = start_slot; i < end_slot; i++) {
                if (!game_state.objects[i].active) {
                    // 设置子弹位置为敌人前方
                    game_state.objects[i].x = game_state.objects[enemy_obj_index].x;
                    game_state.objects[i].y = game_state.objects[enemy_obj_index].y + (SHIP_HEIGHT / 2);
                    game_state.objects[i].active = 1;
                    
                    // 重置冷却时间
                    enemy_cooldowns[enemy_index] = ENEMY_BULLET_COOLDOWN_FRAMES + (rand() % 60);
                    
                    printf("敌人 %d 发射子弹: 位置 x=%d, y=%d\n", 
                           enemy_index, 
                           game_state.objects[i].x,
                           game_state.objects[i].y);
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
    for (i = ENEMY_BULLET_START_INDEX; i < MAX_OBJECTS; i++) {
        if (game_state.objects[i].active) {
            // 敌人子弹向左移动
            game_state.objects[i].x -= ENEMY_BULLET_SPEED;
            
            // 检查是否超出屏幕左侧
            if (game_state.objects[i].x <= 0) {
                // 将子弹设置为非活动状态
                game_state.objects[i].active = 0;
                
                // 重置位置到屏幕外
                game_state.objects[i].x = 0;
                game_state.objects[i].y = 0;
                
                printf("敌人子弹 %d 消失: 越过左边界\n", i);
            }
        }
    }
}

/**
 * 读取控制器输入并更新飞船位置
 */
void update_ship_from_controller(void) {
    // 从控制器读取输入
    libusb_interrupt_transfer(controller, endpoint_address,
        (unsigned char *) &controller_input, sizeof(controller_input), &transferred, 0);

    // 如果读取成功
    if (transferred == sizeof(controller_input)) {
        // 输出控制器状态用于调试
        printf("控制器上下值: %d, 按钮: %d, 摇杆: %d\n", 
               controller_input.ud_arrows, 
               controller_input.buttons, 
               controller_input.bumpers);
        
        // 处理上下移动
        if (controller_input.ud_arrows == 0x00) {
            // 向上移动 - 屏幕坐标系中减小y值
            if (game_state.objects[SHIP_INDEX].y > SHIP_MIN_Y) {
                game_state.objects[SHIP_INDEX].y -= SHIP_VERTICAL_SPEED;
            }
            printf("向上移动: Y=%d\n", game_state.objects[SHIP_INDEX].y);
        } 
        else if (controller_input.ud_arrows == 0xff) {
            // 向下移动 - 屏幕坐标系中增加y值
            if (game_state.objects[SHIP_INDEX].y < SHIP_MAX_Y) {
                game_state.objects[SHIP_INDEX].y += SHIP_VERTICAL_SPEED;
            }
            printf("向下移动: Y=%d\n", game_state.objects[SHIP_INDEX].y);
        }
        
        // 处理射击按钮（Y按钮或肩部按钮）
        if (controller_input.buttons == 0x8f || controller_input.bumpers > 0) {
            fire_bullet();
        }
        
        // 处理随机背景切换（X按钮）
        if (controller_input.buttons == 0x4f) {
            // 确保不会因为按钮持续按下而频繁切换
            int current_time = time(NULL);
            if (current_time - last_random_bg_toggle >= 1) { // 至少间隔1秒
                use_random_bg = !use_random_bg;
                set_random_background_mode(use_random_bg);
                last_random_bg_toggle = current_time;
            }
        }
    }
}

/**
 * Main function - runs the game loop
 */
int main(int argc, char *argv[]) {
    static const char filename[] = "/dev/vga_ball";
    int frame_count = 0;
    int color_index = 0;

    printf("VGA Ball Demo带控制器已启动\n");

    /* Open the device file */
    if ((vga_ball_fd = open(filename, O_RDWR)) == -1) {
        fprintf(stderr, "Could not open %s\n", filename);
        return EXIT_FAILURE;
    }

    /* 处理命令行参数 */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--random-bg") == 0) {
            use_random_bg = 1;
            printf("启用随机背景模式\n");
        }
    }

    /* 打开控制器 */
    if ((controller = opencontroller(&endpoint_address)) == NULL) {
        fprintf(stderr, "无法找到控制器\n");
        close(vga_ball_fd);
        return EXIT_FAILURE;
    }

    /* Initialize random number generator */
    srand(time(NULL));
    
    /* Initialize game state */
    init_game_state();
    update_hardware();
    
    /* 设置随机背景模式 */
    set_random_background_mode(use_random_bg);
    
    printf("开始动画，按Y按钮或肩部按钮发射子弹，上下摇杆移动飞船...\n");
    printf("按X按钮切换随机背景模式\n");
    printf("添加了两个敌人，位置: (%d,%d) 和 (%d,%d)\n", 
           ENEMY1_INITIAL_X, ENEMY1_INITIAL_Y, 
           ENEMY2_INITIAL_X, ENEMY2_INITIAL_Y);
    
    /* Main game loop */
    while (1) {
        /* Decrement cooldown */
        if (bullet_cooldown > 0) bullet_cooldown--;
        
        /* Periodically change background color */
        if (frame_count % 120 == 0 && !use_random_bg) {
            color_index = (color_index + 1) % COLOR_COUNT;
            game_state.background = colors[color_index];
        }
        
        /* 处理控制器输入更新飞船位置 */
        update_ship_from_controller();
        
        /* 敌人尝试发射子弹 */
        enemy_fire_bullet(0);  // 敌人1
        enemy_fire_bullet(1);  // 敌人2
        
        /* Move bullets */
        update_bullets();
        
        /* 更新敌人子弹 */
        update_enemy_bullets();
        
        /* Update the hardware */
        update_hardware();
        
        /* Print debug information */
        if (frame_count % 60 == 0) { // 每秒显示一次状态
            printf("飞船位置: x=%d, y=%d, 随机背景: %s\n", 
                   game_state.objects[SHIP_INDEX].x, 
                   game_state.objects[SHIP_INDEX].y,
                   use_random_bg ? "开启" : "关闭");
            
            // 打印活动子弹数量
            int active_bullet_count = 0;
            for (int i = BULLET_START_INDEX; i < ENEMY_BULLET_START_INDEX; i++) {
                if (game_state.objects[i].active) {
                    active_bullet_count++;
                }
            }
            printf("玩家活动子弹数量: %d\n", active_bullet_count);
            
            // 打印敌人子弹数量
            int enemy_bullet_count = 0;
            for (int i = ENEMY_BULLET_START_INDEX; i < MAX_OBJECTS; i++) {
                if (game_state.objects[i].active) {
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
    
    // 清理资源（在此示例中不会执行到）
    libusb_close(controller);
    close(vga_ball_fd);
    
    return 0;
}