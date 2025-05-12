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
#include <math.h>
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

#define ENEMY_SPACE 10
#define COLUMNS 22

#define LIFE_COUNT 5

#define NUM_ENEMIES 5 // how many different sprites we have

#define TURN_TIME 70


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
static int enemies_moving = 0;
static char row_vals[5] = { 8, 10, 0, 16, 16 };
static char row_sprites[5] = { 2, 3, 0, 4, 4 };

static int enemy_wiggle = 1;
static int enemy_wiggle_time = 0;


static int total_time = 0;


static short turn_x[TURN_TIME] = {1,1,1,1,1,1,1,1,
                    1,1,1,1,1,1,1,1,
                    1,1,1,1,1,1,1,1,
                    1,1,1,1,1,1,1,1,
                    1,1,1,1,1,1,1,1,
                    1,1,1,1,1,1,1,1,
                    0,0,0,0,0,0,0,0,
                    0,0,0,0,0,0,0,0,
                    0,0,0,0,0,0};

static short turn_y[TURN_TIME] = {-1,-1,-1,-1,-1,-1,-1,-1,
                    -1,-1,-1,-1,-1,-1,-1,-1,
                    -1,-1,-1,-1,-1,-1,-1,-1,
                    0,0,0,0,0,0,0,0,
                    1,1,1,1,1,1,1,1,
                    1,1,1,1,1,1,1,1,
                    1,1,1,1,1,1,1,1,
                    1,1,1,1,1,1,1,1,
                    2,2,2,2,2,2};


static int moving = 300;

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

    .ship = {.pos_x = SHIP_INITIAL_X, .pos_y = SHIP_INITIAL_Y, .velo_x = 0, .velo_y = 0, .lives = LIFE_COUNT, .num_bullets = 0, .active = 1},
    .background = {.red = 0xFF, .green = 0x00, .blue = 0xFF},
    .bullets = { 0 },
    .enemies = { 0 }
};

/**
 * Initialize game state
 */
void init_game_state() {

    int space, row = 0, display_row = 1, enemy_count;

    enemy *enemy;

    enemy_count = row_vals[row];

    space = COLUMNS - row_vals[row];

    for (int i = 0, j=0; i < ENEMY_COUNT; i++, j++) {

        enemy = &game_state.enemies[i];

        if (i >= enemy_count){



            if (++row >= 5) break;

            while(row_vals[row] == 0) row++;

            j = 0;
            space = COLUMNS - row_vals[row];
            enemy_count += row_vals[row];
            display_row ++;

        }

        enemy->pos_x = enemy->start_x = 50 + ((ENEMY_WIDTH + ENEMY_SPACE) * (space / 2)) \
                                    + j * (ENEMY_WIDTH + ENEMY_SPACE);
                                    
        enemy->pos_y = enemy->start_y = 30 *display_row;
        enemy->sprite = row_sprites[row];
        enemy->active = 1;
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




void calculate_velo(int x, int y, enemy *enemy, short scaler){

    float new_x, new_y, mag;

    new_x = x - enemy->pos_x;
    new_y = y - enemy->pos_y;

    mag = sqrt(new_x * new_x + new_y * new_y);

    new_x /= mag;
    new_y /= mag;

    new_x *= scaler;
    new_y *= scaler;

    // printf("%f, %f \n", new_x, new_y);

    enemy->velo_x = (int)new_x;
    enemy->velo_y = (int)new_y;

    // printf("%d, %d \n", enemy->velo_x, enemy->velo_y);

}


void turn(enemy *enemy){

    spaceship *ship = &game_state.ship;


    if (enemy->pos_x <= SCREEN_WIDTH/2)
            enemy->velo_x = -turn_x[enemy->turn_counter];
    else
        enemy->velo_x = turn_x[enemy->turn_counter];

    enemy->velo_y = turn_y[enemy->turn_counter];
    enemy->turn_counter++;



    if (enemy->turn_counter == TURN_TIME){


        if(enemy->sprite == 4){

            enemy->velo_x = (enemy->pos_x < SCREEN_WIDTH / 2) ? 2 : -2;
            enemy->velo_y = 1;
        }

        else if(enemy->sprite == 3){

            enemy->velo_x = (enemy->pos_x < SCREEN_WIDTH / 2) ? 3 : -3;
            enemy->velo_y = 2;
        }
        else {

            calculate_velo(ship->pos_x, ship->pos_y, enemy, 2);
        }

    }

}


void enemy_attack(enemy *enemy){


    spaceship *ship = &game_state.ship;
    int cont;


    if (enemy->turn_counter < TURN_TIME)
        turn(enemy);

    else{

        if (enemy->sprite == 2){

            if(++enemy->move_time < 250)
                calculate_velo(ship->pos_x, ship->pos_y, enemy,3);
            else{

                enemy->velo_x = 0;
                enemy->velo_y = 2;
            }
        }

        // else if (enemy->sprite == 3){


        //     if (enemy->start_x < SCREEN_WIDTH/2 && enemy->move_time == 0){

        //         if (enemy->pos_x - ship->pos_x > 10){
                    
        //             calculate_velo(ship->pos_x, ship->pos_y, enemy, 2);
        //             enemy->move_time++;
        //         }
        //     }

        //     else if (enemy->move_time == 0){
        //         if (ship->pos_x - enemy->pos_x > 10){
        //             calculate_velo(ship->pos_x, ship->pos_y, enemy, 2);
        //             enemy->move_time++;
        //         }
        //     }

        //     else{

        //         calculate_velo(ship->pos_x, ship->pos_y, enemy, 2);

        //         if(++ enemy->move_time > 50){

        //             enemy->velo_x = 0;
        //             enemy->velo_y = 2;
        //         }
        //     }

        // }
        // else{

        //     if(++enemy->move_time == 150){

        //         cont = rand() % 3;

        //         if(!cont)
        //             enemy->velo_x = -enemy->velo_x;
        //         else
        //             enemy->move_time --;
        //     }

        //     else if(enemy->move_time == 250){

        //         enemy->velo_x = 0;
        //         enemy->velo_y = 2;
        //     }
        // }

    }

    if (enemy->pos_y > SCREEN_HEIGHT || enemy->pos_x > SCREEN_WIDTH || enemy->pos_x < 0){

        enemy->returning = 1;

        enemy->pos_x = enemy->start_x;
        enemy->pos_y = 0;


        calculate_velo(enemy->start_x + enemy_wiggle_time, enemy->start_y, enemy, 2);
    }

    
    if (enemy->returning){


        if (enemy->pos_x == enemy->start_x + enemy_wiggle_time && enemy->pos_y == enemy->start_y){

            enemy->velo_x = 0;
            enemy->velo_y = 0;

            enemy->moving = 0;
            enemy->returning = 0;
            enemy->move_time = 0;
            enemy->turn_counter = 0;

            enemies_moving --;
        }

        else 
            calculate_velo(enemy->start_x + enemy_wiggle_time, enemy->start_y, enemy, 2);

    }

    enemy->pos_x += enemy->velo_x;
    enemy->pos_y += enemy->velo_y;
}


int enemy_movement(){

    int cont;
    enemy *enemy;

    int rand_enemy = rand() % ENEMY_COUNT;


    for (int i = 0; i < ENEMY_COUNT; i++){

        enemy = &game_state.enemies[i];

        if(!enemy->moving && !enemies_moving && i == rand_enemy && enemy->sprite == 2){

            enemy-> velo_x = 0;
            enemy->velo_y = -4;

            enemy->moving = 1;
            enemies_moving ++;
        }

        if(enemy->moving) enemy_attack(enemy);

        else
            enemy->pos_x += enemy_wiggle;

    }

    return 1;
}


// int enemy_movement(){


//     int rand_enemy = rand() % ENEMY_COUNT;

//     enemy *enemy;
//     int num_left = 0;
//     float mag, new_x, new_y;
//     bullet *bul;
//     spaceship *ship = &game_state.ship;

    

//     for (int i = 0; i < ENEMY_COUNT; i++){

//         enemy = &game_state.enemies[i];
//         bul = &enemy->bul;
        
//         if (enemy_moving == 0 && i == rand_enemy){

//             enemy->velo_y = 2;
//             enemy->velo_x = 0;
//             enemy_moving ++;

//         }

//         if (enemy->velo_y != 0 || enemy->velo_x != 0){

//             enemy->pos_x += enemy->velo_x;
//             enemy->pos_y += enemy->velo_y;

//             if(enemy->returning){

//                 if (enemy->pos_x == 20 + i*(ENEMY_WIDTH + ENEMY_SPACE) && enemy->pos_y == 50){

//                     enemy->velo_x = 0;
//                     enemy->velo_y = 0;

//                     enemy->returning = 0;

//                     enemy_moving --;

//                 }
//             }

//             else if (enemy->pos_y >= SCREEN_HEIGHT - 5){

//                 enemy->pos_x = 20 + i*(ENEMY_WIDTH + ENEMY_SPACE);
//                 enemy->pos_y = 0;

//                 enemy->velo_x = 0;
//                 enemy->velo_y = 2;

//                 enemy->returning = 1;

//                 moving = 300;

//             }

//             // else if (enemy->pos_y >= ship->pos_y){
//             //     enemy->velo_x = 0;
//             //     enemy->velo_y = 2;

//             //     printf("%d, %d \n", enemy->pos_y, ship->pos_y);
//             // }

//             else{
//                 if (--moving <= 0){

//                     enemy->velo_x = 0;
//                     enemy->velo_y = 2;

//                 }

//                 else{

//                     new_x = ship->pos_x - enemy->pos_x;
//                     new_y = ship->pos_y - enemy->pos_y;

//                     mag = sqrt(new_x * new_x + new_y * new_y);

//                     // printf("%f, %f, %f \n", new_x, new_y, mag);

//                     new_x /= mag;
//                     new_y /= mag;

//                     new_x *= 3;
//                     new_y *= 3;

//                     enemy->velo_x = (int)new_x;
//                     enemy->velo_y = (int)new_y;
//                 }
//             }

//             continue;
//         }


//         if (enemy->bul.active){

//             bul->pos_y += bul->velo_y;

//             if (abs(game_state.ship.pos_x - bul->pos_x) <= SHIP_WIDTH
//             && abs(game_state.ship.pos_y - bul->pos_y) <= SHIP_HEIGHT){

//                 bul->active = 0;
//                 game_state.ship.lives -= 1;
//             }

//             if(bul->pos_y > SCREEN_HEIGHT) bul->active = 0;
//         }

//         if (enemy->active){

//             // if (!enemy->bul.active && abs(game_state.ship.pos_x - enemy->pos_x) < 10){

//             //     bul->active = 1;
//             //     bul->pos_x = enemy->pos_x+(ENEMY_WIDTH/2); // make it start in the middle of the ship
//             //     bul->pos_y = enemy->pos_y+(SHIP_HEIGHT+ (SHIP_HEIGHT/2)); // make it start above the ship
//             //     bul->velo_y = 3; // towards the top of the screen
//             // }
//             num_left ++;

//             // important!!! compare to whichever has the larger size
//             if (abs(game_state.ship.pos_x - enemy->pos_x) <= SHIP_WIDTH
//             && abs(game_state.ship.pos_y - enemy->pos_y) <= SHIP_HEIGHT){

//                 enemy->active = 0;
//                 game_state.ship.lives -= 1;
//                 num_left --;
//             }
//         }
//     }

//     return num_left;
// }

struct libusb_device_handle *controller;

uint8_t endpoint_address;


int main(){

    spaceship *ship = &game_state.ship;
    controller_packet packet;
    int transferred, start = 0, new_bullet, prev_bullet = 0, enemies_remaining;
    int bumpers = 0, buttons = 0;

    srand((unsigned)time(NULL));


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

        if (++total_time%3 == 0) enemy_wiggle_time += enemy_wiggle;
        if (abs(enemy_wiggle_time) == 20) enemy_wiggle = -enemy_wiggle;


        printf("%d, %d, %d \n", total_time, enemy_wiggle_time, enemy_wiggle_time);





        

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

            if(!enemies_remaining){
                printf("You Won!");
                break;
            }


            update_hardware();

            usleep(16000);
        }    
    }

}
