/*
 * Userspace program for the VGA Ball game
 * With image loading capability
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

/* 添加处理BMP图片所需的库 */
#include <stdint.h>

/* File descriptor for the VGA ball device */
int vga_ball_fd;

/* Screen dimensions */
#define SCREEN_WIDTH 640  // 修改为显示分辨率的一半，因为hcount[10:1]
#define SCREEN_HEIGHT 480

/* Ship constants */
#define SHIP_WIDTH 40
#define SHIP_HEIGHT 30
#define SHIP_INITIAL_X 200
#define SHIP_INITIAL_Y 240

/* Bullet constants */
#define BULLET_SIZE 4
#define BULLET_SPEED 10

/* Animation and game timing */
#define FRAME_DELAY_MS 16  // ~60 FPS
#define BULLET_COOLDOWN_FRAMES 20 // 0.33 second between shots

/* Game state */
vga_ball_arg_t game_state;
int bullet_cooldown = 0;

/* BMP文件头结构 */
#pragma pack(push, 1)
typedef struct {
    uint16_t signature;      // "BM"
    uint32_t fileSize;       // 文件大小
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t dataOffset;     // 图像数据偏移量
} BMPFileHeader;

typedef struct {
    uint32_t headerSize;     // 信息头大小
    int32_t width;           // 图像宽度
    int32_t height;          // 图像高度
    uint16_t planes;         // 颜色平面数
    uint16_t bitsPerPixel;   // 每像素位数
    uint32_t compression;    // 压缩方式
    uint32_t imageSize;      // 图像数据大小
    int32_t xPixelsPerMeter;
    int32_t yPixelsPerMeter;
    uint32_t colorsUsed;
    uint32_t colorsImportant;
} BMPInfoHeader;
#pragma pack(pop)

/**
 * 读取BMP图片并存储到游戏状态中 - 简化版本，只加载部分图片数据
 */
int load_bmp_image(const char *filename, int x, int y) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "无法打开图片文件: %s\n", filename);
        return -1;
    }
    
    BMPFileHeader fileHeader;
    BMPInfoHeader infoHeader;
    
    // 读取文件头
    if (fread(&fileHeader, sizeof(fileHeader), 1, file) != 1) {
        fprintf(stderr, "读取文件头失败\n");
        fclose(file);
        return -1;
    }
    
    // 检查是否是BMP文件
    if (fileHeader.signature != 0x4D42) { // "BM"
        fprintf(stderr, "不是有效的BMP文件\n");
        fclose(file);
        return -1;
    }
    
    // 读取信息头
    if (fread(&infoHeader, sizeof(infoHeader), 1, file) != 1) {
        fprintf(stderr, "读取信息头失败\n");
        fclose(file);
        return -1;
    }
    
    // 检查格式支持
    if (infoHeader.bitsPerPixel != 24) {
        fprintf(stderr, "仅支持24位BMP图片\n");
        fclose(file);
        return -1;
    }
    
    // 设置图片位置
    game_state.image.x = x;
    game_state.image.y = y;
    game_state.image.display = 1;
    
    // 计算每行的字节数（包括填充）
    int rowSize = ((infoHeader.width * 3 + 3) & ~3);
    
    // 移动到图像数据起始位置
    fseek(file, fileHeader.dataOffset, SEEK_SET);
    
    // 读取并转换图像数据
    // BMP存储是从下到上的，需要反转
    uint8_t *rowBuffer = (uint8_t*)malloc(rowSize);
    
    // 限制图片大小不超过我们的缓冲区
    int width = (infoHeader.width < IMAGE_WIDTH) ? infoHeader.width : IMAGE_WIDTH;
    int height = (infoHeader.height < IMAGE_HEIGHT) ? infoHeader.height : IMAGE_HEIGHT;
    
    // 初始化图像数据为0
    memset(game_state.image.data, 0, sizeof(game_state.image.data));
    
    // BMP图片是从下到上存储的，所以我们需要反转
    for (int y = height - 1; y >= 0; y--) {
        // 读取一行数据
        if (fread(rowBuffer, rowSize, 1, file) != 1) {
            fprintf(stderr, "读取图像数据失败\n");
            free(rowBuffer);
            fclose(file);
            return -1;
        }
        
        // 复制到图像缓冲区，注意BMP是BGR格式，需要转为RGB
        for (int x = 0; x < width; x++) {
            game_state.image.data[height - 1 - y][x][0] = rowBuffer[x * 3 + 2]; // R
            game_state.image.data[height - 1 - y][x][1] = rowBuffer[x * 3 + 1]; // G
            game_state.image.data[height - 1 - y][x][2] = rowBuffer[x * 3 + 0]; // B
        }
    }
    
    free(rowBuffer);
    fclose(file);
    printf("成功加载图片: %s (%dx%d)\n", filename, width, height);
    
    return 0;
}

/**
 * 传输图片数据到硬件 - 简化版本
 */
void update_image() {
    // 只更新图片位置和显示状态
    if (ioctl(vga_ball_fd, VGA_BALL_WRITE_IMAGE, &game_state)) {
        perror("ioctl(VGA_BALL_WRITE_IMAGE) failed");
        exit(EXIT_FAILURE);
    }
}

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
    
    // Set background color
    game_state.background.red = 0x00;
    game_state.background.green = 0x00;
    game_state.background.blue = 0x20;  // Dark blue background
    
    // Ship (left side)
    game_state.ship.position.x = SHIP_INITIAL_X;
    game_state.ship.position.y = SHIP_INITIAL_Y;
    game_state.ship.active = 1;
    
    // 初始化所有子弹为非激活状态
    for (i = 0; i < MAX_BULLETS; i++) {
        game_state.bullets[i].position.x = 0;
        game_state.bullets[i].position.y = 0;
        game_state.bullets[i].active = 0;
    }
    
    // 初始化图片数据
    memset(&game_state.image, 0, sizeof(game_state.image));
    game_state.image.display = 0;  // 默认不显示图片
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
        for (i = 0; i < MAX_BULLETS; i++) {
            if (!game_state.bullets[i].active) {
                // 设置子弹位置为飞船前方
                game_state.bullets[i].position.x = game_state.ship.position.x + SHIP_WIDTH;
                game_state.bullets[i].position.y = game_state.ship.position.y + (SHIP_HEIGHT / 2);
                game_state.bullets[i].active = 1;
                
                // 重置冷却时间
                bullet_cooldown = BULLET_COOLDOWN_FRAMES;
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
            game_state.bullets[i].position.x += BULLET_SPEED;
            
            // 检查是否超出屏幕
            if (game_state.bullets[i].position.x > SCREEN_WIDTH) {
                game_state.bullets[i].active = 0;
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
    
    printf("VGA Ball Demo with Image Support started\n");

    /* Open the device file */
    if ((vga_ball_fd = open(filename, O_RDWR)) == -1) {
        fprintf(stderr, "Could not open %s\n", filename);
        return EXIT_FAILURE;
    }

    /* Initialize random number generator */
    srand(time(NULL));
    
    /* Initialize game state */
    init_game_state();
    
    /* 处理命令行参数 - 如果提供了图片路径，加载图片 */
    if (argc > 1) {
        printf("尝试加载图片: %s\n", argv[1]);
        if (load_bmp_image(argv[1], 100, 100) == 0) {
            // 成功加载图片，将其传输到硬件
            update_image();
        }
    }
    
    /* 更新所有游戏状态 */
    update_hardware();
    
    printf("Starting animation, press Ctrl+C to exit...\n");
    
  /* Main game loop */
    while (1) {
        /* Decrement cooldown */
        if (bullet_cooldown > 0) bullet_cooldown--;
        
        /* Periodically change background color */
        if (frame_count % 120 == 0) {  // Every 2 seconds
            color_index = (color_index + 1) % COLOR_COUNT;
            game_state.background = colors[color_index];
        }
        
        /* Always try to fire bullet when cooldown allows */
        fire_bullet();
        
        /* Move bullets */
        update_bullets();
        
        /* Vertical ship movement (small oscillation) */
        if (frame_count % 300 < 150) {
            game_state.ship.position.y++;
        } else {
            game_state.ship.position.y--;
        }
        
        /* Update the hardware */
        update_hardware();
        
        /* Delay for next frame */
        usleep(FRAME_DELAY_MS * 1000);
        
        /* Increment frame counter */
        frame_count++;
    }
    
    return 0;
}