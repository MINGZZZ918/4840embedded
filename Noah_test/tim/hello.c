/*
 * CSEE 4840 Lab 2 for 2019
 *
 * Name/UNI:Noah Hartzfeld (nah2178)
            Zhengtao Hu (zh2651)
            Mingzhi Li (ml5160)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/ioctl.h> 
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <fcntl.h>
#include "vga_ball.h"
#include "controller.h"


// #define SCREEN_WIDTH 1280
#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480

#define COLOR_COUNT 5

#define SHIP_WIDTH 16
#define SHIP_HEIGHT 16
#define SHIP_INITIAL_X 200
#define SHIP_INITIAL_Y 240

#define ENEMY_WIDTH 16
#define ENEMY_HEIGHT 16

#define LIFE_COUNT 5

#define NUM_ENEMIES 5 // how many different sprites we have


#define LEFT_ARROW 0x00
#define RIGHT_ARROW 0xff
#define UP_ARROW 0x00
#define DOWN_ARROW 0xff
#define BUTTON_A 0x2f
#define Y_BUTTON 0x8f
#define LEFT_BUMPER 0x01
#define RIGHT_BUMPER 0x02
#define LR_BUMPER 0x03

#define NO_INPUT 0x7f // ??????????????????

/* File descriptor for the VGA ball device */
static int vga_ball_fd;

static const char filename[] = "/dev/vga_ball";

/* Array of background colors to cycle through */
static const background_color colors[] = {
    { 0x00, 0x00, 0x10 },  // Very dark blue
    { 0x00, 0x00, 0x20 },  // Dark blue
    { 0x10, 0x10, 0x30 },  // Navy blue
    { 0x00, 0x00, 0x40 },  // Medium blue
    { 0x20, 0x20, 0x40 }   // Blue-purple
};

static gamestate game_state = {

    .ship = {.pos_x = SHIP_INITIAL_X, .pos_y = SHIP_INITIAL_Y, .velo_x = 0, .velo_y = 0, .lives = LIFE_COUNT, .num_bullets = 0, .sprite = 0, .active = 1},
    .background = {.red = 0xFF, .green = 0xFF, .blue = 0xFF},
    .bullets = { 0 },
    .enemies = { 0 }
};

/**
 * Initialize game state
 */
void init_game_state() {

    for (int i = 0; i < ENEMY_COUNT; i++) {

        game_state.enemies[i].pos_x = 20 + i*(ENEMY_WIDTH + 44);
        game_state.enemies[i].pos_y = 50;
        game_state.enemies[i].active = 1;

        game_state.enemies[i].bul.pos_x = 0;
        game_state.enemies[i].bul.pos_y = 0;
        game_state.enemies[i].bul.active = 0;
    }
}

/**
 * Update game state and send to the device
 */
void update_hardware() {
    if (ioctl(vga_ball_fd, VGA_BALL_UPDATE_GAME_STATE, &game_state)) {
        perror("ioctl(VGA_BALL_UPDATE_GAME_STATE) failed");
        exit(EXIT_FAILURE);
    }
}

void ship_movement(){

    spaceship *ship = &game_state.ship;

    ship->pos_x += ship->velo_x;
    ship->pos_y += ship->velo_y;
}

void bullet_movement(int new_bullet){

    bullet *bul;
    enemy *enemy;

    for (int i = 0; i < MAX_BULLETS; i++) {

        bul = &game_state.bullets[i];

        if (bul->active){

            bul->pos_y += bul->velo_y;
            if (bul->pos_y < 5){ // top of screen

                bul->active = 0;
                game_state.ship.num_bullets --;
                continue;
            }

            for (int j = 0; j<ENEMY_COUNT; j++){ // checking to see if we hit an enemy

                enemy = &game_state.enemies[j];

                if (enemy->active && abs(enemy->pos_x - bul->pos_x) <= ENEMY_WIDTH
                    && abs(enemy->pos_y - bul->pos_y) <= ENEMY_HEIGHT){

                    enemy->active = 0;
                    bul->active = 0;
                    game_state.ship.num_bullets --;
                    break;
                }
            }
        }

        else if (!bul->active && new_bullet) {
            bul->active = 1;
            bul->pos_x = game_state.ship.pos_x+(SHIP_WIDTH/2); // make it start in the middle of the ship
            bul->pos_y = game_state.ship.pos_y-(SHIP_HEIGHT/2); // make it start above the ship
            bul->velo_y = -3; // towards the top of the screen
            game_state.ship.num_bullets ++;
            new_bullet = 0;
        }
    }
}

int enemy_movement(){

    enemy *enemy;
    int num_left = 0;
    bullet *bul;

    for (int i = 0; i < ENEMY_COUNT; i++){

        enemy = &game_state.enemies[i];
        bul = &enemy->bul;

        if (enemy->bul.active){

            bul->pos_y += bul->velo_y;

            if (abs(game_state.ship.pos_x - bul->pos_x) <= SHIP_WIDTH
            && abs(game_state.ship.pos_y - bul->pos_y) <= SHIP_HEIGHT){

                bul->active = 0;
                game_state.ship.lives -= 1;
            }

            if(bul->pos_y > SCREEN_HEIGHT) bul->active = 0;
        }

        if (enemy->active){

            if (!enemy->bul.active && abs(game_state.ship.pos_x - enemy->pos_x) < 10){

                bul->active = 1;
                bul->pos_x = enemy->pos_x+(ENEMY_WIDTH/2); // make it start in the middle of the ship
                bul->pos_y = enemy->pos_y+(SHIP_HEIGHT+ (SHIP_HEIGHT/2)); // make it start above the ship
                bul->velo_y = 3; // towards the top of the screen
            }
            num_left ++;

            // important!!! compare to whichever has the larger size
            if (abs(game_state.ship.pos_x - enemy->pos_x) <= SHIP_WIDTH
            && abs(game_state.ship.pos_y - enemy->pos_y) <= SHIP_HEIGHT){

                enemy->active = 0;
                game_state.ship.lives -= 1;
                num_left --;
            }
        }
    }

    return num_left;
}

struct libusb_device_handle *controller;

uint8_t endpoint_address;


int main(){

    spaceship *ship = &game_state.ship;
    controller_packet packet;
    int transferred, start = 0, new_bullet, prev_bullet = 0, enemies_remaining;
    int bumpers = 0, buttons = 0;

    /* Open the device file */
    if ((vga_ball_fd = open(filename, O_RDWR)) == -1) {
        fprintf(stderr, "Could not open %s\n", filename);
        return EXIT_FAILURE;
    }

    /* Open the controller */
    if ( (controller = opencontroller(&endpoint_address)) == NULL ) {
        fprintf(stderr, "Did not find a controller\n");
        exit(1);
    }

    printf("Press A \n");

    while (start == 0){
        // recieve packets 
        libusb_interrupt_transfer(controller, endpoint_address,
            (unsigned char *) &packet, sizeof(packet), &transferred, 0);

        if(packet.buttons == BUTTON_A) start = 1;
    }

    printf("Game Begins! \n");

    init_game_state();
    update_hardware();
    for (;;){       

        new_bullet = 0;

        if (ship->lives == 0) break;

        libusb_interrupt_transfer(controller, endpoint_address,
            (unsigned char *) &packet, sizeof(packet), &transferred, 0);
            
        if (transferred == sizeof(packet)) {

            switch (packet.lr_arrows) {
                case LEFT_ARROW:
                    if(ship->pos_x > 0)
                        ship->velo_x = -2;

                    // printf("%d, %d \n", ship->pos_x, ship->pos_y);
                    break;
                    
                case RIGHT_ARROW:
                    if(ship->pos_x < SCREEN_WIDTH-SHIP_WIDTH)
                        ship->velo_x = 2;

                    // printf("%d, %d \n", ship->pos_x, ship->pos_y);
                    break;

                default:
                    ship->velo_x = 0;
                    break;
            }

            switch (packet.ud_arrows) {
                case UP_ARROW:
                    if (ship->pos_y < SCREEN_HEIGHT - 5)
                        ship->velo_y = -2;

                    // printf("%d, %d\n", ship->pos_x, ship->pos_y);
                    break;
                    
                case DOWN_ARROW:
                    if (ship->pos_y > 0+SHIP_HEIGHT)
                        ship->velo_y = 2;

                    // printf("%d, %d \n", ship->pos_x, ship->pos_y);
                    break;

                default:
                    ship->velo_y = 0;
                    break;
            }

            switch (packet.buttons) {                
                case Y_BUTTON:
                    if (!prev_bullet && ship->num_bullets < MAX_BULLETS){
                        new_bullet = 1; // do not allow them to hold the button to shoot
                        prev_bullet = 1;
                    }

                    buttons = 1;
                    // printf("Bullet \n");
                    break;

                default:
                    if (!bumpers) prev_bullet = 0;
                    buttons = 0;
                    break;
            }

            switch (packet.bumpers) {
                case LEFT_BUMPER:
                    if (!prev_bullet && ship->num_bullets < MAX_BULLETS){
                        new_bullet = 1; // do not allow them to hold the button to shoot
                        prev_bullet = 1;
                    }

                    bumpers = 1;

                    break;
                    
                case RIGHT_BUMPER:
                    if (!prev_bullet && ship->num_bullets < MAX_BULLETS){
                        new_bullet = 1; // do not allow them to hold the button to shoot
                        prev_bullet = 1;
                    }

                    bumpers = 1;

                    break;

                case LR_BUMPER:
                    if (!prev_bullet && ship->num_bullets < MAX_BULLETS){
                        new_bullet = 1; // do not allow them to hold the button to shoot
                        prev_bullet = 1;
                    }
                    bumpers = 1;
                    break;

                default:
                    if (!buttons) prev_bullet = 0; // only reset bullets if the y button has not been pressed
                    bumpers = 0;
                    // printf("bumpers\n");
                    break;
            }

            ship_movement();
            bullet_movement(new_bullet);
            enemies_remaining = enemy_movement();

            if(ship->lives <= 0){
                printf("You lost =( \n");
                break;
            }

            update_hardware();

            if(!enemies_remaining){
                printf("You Won!");
                break;
            }

            usleep(16000);
        }    
    }

}

