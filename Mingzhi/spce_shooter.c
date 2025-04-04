/*
 * Userspace program for the Space Shooter game
 * Demonstrates two spaceships firing bullets at each other
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
#include "vga_spaceship.h"

/* File descriptor for the VGA spaceship device */
int vga_spaceship_fd;

/* Screen dimensions */
#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 480

/* Ship constants */
#define SHIP_WIDTH 40
#define SHIP_HEIGHT 30
#define SHIP1_INITIAL_X 200
#define SHIP2_INITIAL_X 1000
#define SHIP_INITIAL_Y 240

/* Bullet constants */
#define BULLET_SIZE 4
#define BULLET1_SPEED 5
#define BULLET2_SPEED -5  // Negative because ship 2 fires to the left

/* Animation and game timing */
#define FRAME_DELAY_MS 16  // ~60 FPS
#define COOLDOWN_FRAMES 60 // 1 second between shots

/* Game state */
vga_spaceship_arg_t game_state;
int ship1_cooldown = 0;
int ship2_cooldown = 0;

/* Array of background colors to cycle through */
static const vga_spaceship_color_t colors[] = {
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
    // Set background color
    game_state.background.red = 0x00;
    game_state.background.green = 0x00;
    game_state.background.blue = 0x20;  // Dark blue background
    
    // Ship 1 (left side)
    game_state.ship1.position.x = SHIP1_INITIAL_X;
    game_state.ship1.position.y = SHIP_INITIAL_Y;
    game_state.ship1.active = 1;
    
    // Ship 2 (right side)
    game_state.ship2.position.x = SHIP2_INITIAL_X;
    game_state.ship2.position.y = SHIP_INITIAL_Y;
    game_state.ship2.active = 1;
    
    // Bullets (initially inactive)
    game_state.bullet1.position.x = 0;
    game_state.bullet1.position.y = 0;
    game_state.bullet1.active = 0;
    
    game_state.bullet2.position.x = 0;
    game_state.bullet2.position.y = 0;
    game_state.bullet2.active = 0;
}

/**
 * Update game state and send to the device
 */
void update_hardware(void) {
    if (ioctl(vga_spaceship_fd, VGA_SPACESHIP_UPDATE_GAME_STATE, &game_state)) {
        perror("ioctl(VGA_SPACESHIP_UPDATE_GAME_STATE) failed");
        exit(EXIT_FAILURE);
    }
}

/**
 * Try to fire a bullet from ship 1 if cooldown allows
 */
void try_fire_ship1_bullet(void) {
    if (ship1_cooldown <= 0 && !game_state.bullet1.active) {
        // Set bullet position to front of ship
        game_state.bullet1.position.x = game_state.ship1.position.x + SHIP_WIDTH;
        game_state.bullet1.position.y = game_state.ship1.position.y + (SHIP_HEIGHT / 2);
        game_state.bullet1.active = 1;
        
        // Reset cooldown
        ship1_cooldown = COOLDOWN_FRAMES;
    }
}

/**
 * Try to fire a bullet from ship 2 if cooldown allows
 */
void try_fire_ship2_bullet(void) {
    if (ship2_cooldown <= 0 && !game_state.bullet2.active) {
        // Set bullet position to front of ship (facing left)
        game_state.bullet2.position.x = game_state.ship2.position.x - BULLET_SIZE;
        game_state.bullet2.position.y = game_state.ship2.position.y + (SHIP_HEIGHT / 2);
        game_state.bullet2.active = 1;
        
        // Reset cooldown
        ship2_cooldown = COOLDOWN_FRAMES;
    }
}

/**
 * Update bullet positions and check for out-of-bounds
 */
void update_bullets(void) {
    // Update bullet1 if active
    if (game_state.bullet1.active) {
        game_state.bullet1.position.x += BULLET1_SPEED;
        
        // Check if out of bounds
        if (game_state.bullet1.position.x > SCREEN_WIDTH) {
            game_state.bullet1.active = 0;
        }
    }
    
    // Update bullet2 if active
    if (game_state.bullet2.active) {
        game_state.bullet2.position.x += BULLET2_SPEED;
        
        // Check if out of bounds
        if (game_state.bullet2.position.x < 0) {
            game_state.bullet2.active = 0;
        }
    }
}

/**
 * Main function - runs the game loop
 */
int main(void) {
    static const char filename[] = "/dev/vga_spaceship";
    int frame_count = 0;
    int color_index = 0;

    printf("Space Shooter Demo started\n");

    /* Open the device file */
    if ((vga_spaceship_fd = open(filename, O_RDWR)) == -1) {
        fprintf(stderr, "Could not open %s\n", filename);
        return EXIT_FAILURE;
    }

    /* Initialize random number generator */
    srand(time(NULL));
    
    /* Initialize game state */
    init_game_state();
    update_hardware();
    
    printf("Starting animation, press Ctrl+C to exit...\n");
    
    /* Main game loop */
    while (1) {
        /* Decrement cooldowns */
        if (ship1_cooldown > 0) ship1_cooldown--;
        if (ship2_cooldown > 0) ship2_cooldown--;
        
        /* Periodically change background color */
        if (frame_count % 120 == 0) {  // Every 2 seconds
            color_index = (color_index + 1) % COLOR_COUNT;
            game_state.background = colors[color_index];
        }
        
        /* Fire bullets randomly to demonstrate shooting */
        if (rand() % 120 == 0) {  // Randomize firing
            try_fire_ship1_bullet();
        }
        
        if (rand() % 120 == 0) {  // Randomize firing
            try_fire_ship2_bullet();
        }
        
        /* Move bullets */
        update_bullets();
        
        /* Vertical ship movement (small oscillation) */
        if (frame_count % 300 < 150) {
            game_state.ship1.position.y++;
            game_state.ship2.position.y--;
        } else {
            game_state.ship1.position.y--;
            game_state.ship2.position.y++;
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