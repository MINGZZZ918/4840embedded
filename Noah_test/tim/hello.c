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

#define ENEMY3_BULLET_COOLDOWN 50

#define ENEMY4_BULLET_COOLDOWN 40


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

static char row_vals[5] = { 2, 6, 8, 10, 10 };
static char row_sprites[5] = { 2, 3, 3, 4, 4 };


static int row_fronts[5];
static int row_backs[5];


static int enemy_wiggle = 1;
static int enemy_wiggle_time = 0;

static int round_num = 1;
static long round_time = 0;


static short turn_x[TURN_TIME] = {2,2,2,2,2,2,2,2,
                    2,2,2,2,2,2,2,2,
                    2,2,2,2,2,2,2,2,
                    2,2,2,2,2,2,2,2,
                    2,2,2,2,2,2,2,2,
                    2,2,2,2,2,2,2,2,
                    0,0,0,0,0,0,0,0,
                    0,0,0,0,0,0,0,0,
                    0,0,0,0,0,0};

static short turn_y[TURN_TIME] = {-2,-2,-2,-2,-2,-2,-2,-2,
                    -2,-2,-2,-2,-2,-2,-2,-2,
                    -2,-2,-2,-2,-2,-2,-2,-2,
                    0,0,0,0,0,0,0,0,
                    2,2,2,2,2,2,2,2,
                    2,2,2,2,2,2,2,2,
                    2,2,2,2,2,2,2,2,
                    2,2,2,2,2,2,2,2,
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

    int space, row = 0, enemy_count;

    enemy *enemy;

    enemy_count = row_vals[row];

    space = COLUMNS - row_vals[row];

    row_fronts[row] = 0;

    for (int i = 0, j=0; i < ENEMY_COUNT; i++, j++) {

        enemy = &game_state.enemies[i];

        if (i >= enemy_count){

            row_backs[row] = i-1;

            if (++row >= 5) break;

            j = 0;
            space = COLUMNS - row_vals[row];
            enemy_count += row_vals[row];


            row_fronts[row] = i;
        }

        enemy->pos_x = enemy->start_x = 50 + ((ENEMY_WIDTH + ENEMY_SPACE) * (space / 2)) \
                                    + j * (ENEMY_WIDTH + ENEMY_SPACE);
                                    
        enemy->pos_y = enemy->start_y = 60 + 30 *(row+1);
        enemy->sprite = row_sprites[row];
        enemy->pos_num = i;
        enemy->active = 1;

        enemy->bul1 = enemy->bul2 = -1;
        enemy->row = row;
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



void calculate_velo(int ship_x, int ship_y, void *object, int type, short scaler){

    float new_x, new_y, mag;
    enemy *enemyy;
    bullet *bul;

    if (type) {

        enemyy = (enemy *)object;

        new_x = ship_x - enemyy->pos_x;
        new_y = ship_y - enemyy->pos_y;
    }

    else {

        bul = (bullet *)object;

        new_x = ship_x - bul->pos_x;
        new_y = ship_y - bul->pos_y;
    }

    mag = sqrt(new_x * new_x + new_y * new_y);

    new_x /= mag;
    new_y /= mag;

    new_x *= scaler;
    new_y *= scaler;

    if (type) {

        enemyy->velo_x = (short)new_x;
        enemyy->velo_y = (short)new_y;
    }
    else{

        bul->velo_x = (short)new_x;
        bul->velo_y = (short)new_y;
    }
}


void turn(enemy *enemy){

    spaceship *ship = &game_state.ship;


    if (enemy->start_x <= SCREEN_WIDTH/2)
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

            calculate_velo(ship->pos_x, ship->pos_y, enemy, 1, 2);
        }

    }

}



bool aquire_bullet(enemy *enemy, int bul_num){

    bullet *bul;
    
    for (int i = 0; i<MAX_BULLETS; i++){

        bul = &game_state.bullets[i];

        if(!bul->active){

            printf("NOT ACTIVE");

            bul->active = 1;

            bul->pos_x = enemy->pos_x+(ENEMY_WIDTH/2);
            bul->pos_y = enemy->pos_y+(ENEMY_HEIGHT);

            bul->enemy = enemy->pos_num;

            if (bul_num == 1) enemy->bul1 = i;
            else enemy->bul2 = i;

            return 1;
        }
    }

    return 0;


}


void move_enemy_bul(enemy *enemy, int bul_num){

    spaceship *ship = &game_state.ship;

    bullet *bul;

    if (bul_num == 1) bul = &game_state.bullets[enemy->bul1];
    else bul = &game_state.bullets[enemy->bul2];


    bul->pos_x += bul->velo_x;
    bul->pos_y += bul->velo_y;


    if (abs(ship->pos_x - bul->pos_x) <= SHIP_WIDTH
                && abs(ship->pos_y - bul->pos_y) <= SHIP_HEIGHT){

            bul->active = 0;
            bul->enemy = -1;

            if (bul_num == 1) enemy->bul1 = -1;
            else enemy->bul2 = -1;

            ship->lives --;
        }

    if (bul->active && bul->pos_y >= SCREEN_HEIGHT || bul->pos_x >= SCREEN_WIDTH || bul->pos_x < 0){

        bul->active = 0;
        bul->enemy = -1;

        if (bul_num == 1) enemy->bul1 = -1;
        else enemy->bul2 = -1;

    }
}


void enemy_shoot(enemy *enemy){

    spaceship *ship = &game_state.ship;

    bool aquired;

    if (enemy->bul_cooldown <= 0 && enemy->turn_counter >= TURN_TIME && !enemy->returning){

        if (enemy->sprite == 3){

            if (abs(ship->pos_x - enemy->pos_x) <= 40
                    && abs(ship->pos_y - enemy->pos_y) <= 150
                    && ship->pos_y - 60 > enemy->pos_y){

                if (enemy->bul1 == -1){

                    aquired = aquire_bullet(enemy, 1);

                    if(aquired){
                        enemy->bul_cooldown = ENEMY3_BULLET_COOLDOWN;
                        game_state.bullets[enemy->bul1].velo_y = 3;

                    }
                }
                // else if (enemy->bul2 == -1){

                //     aquired = aquire_bullet(enemy, 2);

                //     if(aquired){
                //         enemy->bul_cooldown = ENEMY3_BULLET_COOLDOWN*2;
                //         game_state.bullets[enemy->bul2].velo_y = 3;

                //     }
                // }
            }
        }

        if(enemy->sprite == 4){

            if (abs(ship->pos_x - enemy->pos_x) <= 150
                && abs(ship->pos_y - enemy->pos_y <= 200
                && ship->pos_y - 60 > enemy->pos_y)){

                if (enemy->bul1 == -1){

                    aquired = aquire_bullet(enemy, 1);

                    if(aquired){

                        enemy->bul_cooldown = ENEMY4_BULLET_COOLDOWN;
                        calculate_velo(ship->pos_x, ship->pos_y, &game_state.bullets[enemy->bul1], 0, 4);

                    }
                }
                // else if (enemy->bul2 == -1){

                //     aquired = aquire_bullet(enemy, 2);

                //     if(aquired){
                //         enemy->bul_cooldown = ENEMY4_BULLET_COOLDOWN;
                //         calculate_velo(ship->pos_x, ship->pos_y, &game_state.bullets[enemy->bul2], 0, 4);
                //     }   
                // }
            }
        }
    }

    else if(enemy->turn_counter <= TURN_TIME)
        enemy->bul_cooldown --;


    if(game_state.bullets[enemy->bul1].active)
        move_enemy_bul(enemy, 1);

    if(game_state.bullets[enemy->bul2].active)
        move_enemy_bul(enemy, 2);

}


void enemy_attack(enemy *enemy){


    spaceship *ship = &game_state.ship;
    int cont;


    if (enemy->turn_counter < TURN_TIME)
        turn(enemy);

    else{

        if (enemy->sprite == 2){

            if(++enemy->move_time < 200)
                calculate_velo(ship->pos_x, ship->pos_y, enemy, 1, 3);
            else{

                enemy->velo_x = 0;
                enemy->velo_y = 2;
            }
        }

        else if (enemy->sprite == 3){


            if (enemy->start_x < SCREEN_WIDTH/2 && enemy->move_time == 0){

                if (enemy->pos_x - ship->pos_x > 10){
                    
                    calculate_velo(ship->pos_x, ship->pos_y, enemy, 1, 3);
                    enemy->move_time++;
                }
            }

            else if (enemy->move_time == 0){

                if (ship->pos_x - enemy->pos_x > 10){

                    calculate_velo(ship->pos_x, ship->pos_y, enemy, 1, 3);
                    enemy->move_time++;
                }
            }

            else{

                if(enemy->move_time < 75)
                    calculate_velo(ship->pos_x -200, ship->pos_y, enemy, 1, 2);

                else
                    calculate_velo(ship->pos_x +200, ship->pos_y, enemy, 1, 2);

                if(++ enemy->move_time > 150){

                    enemy->velo_x = (enemy->pos_x > ship->pos_x) ? 1 : -1;
                    enemy->velo_y = 2;
                }
            }

        }
        else{

            if(++enemy->move_time == 150){

                cont = rand() % 3;

                if(!cont)
                    enemy->velo_x = -enemy->velo_x;
                else
                    enemy->move_time --;
            }

            else if(enemy->move_time == 250){

                enemy->velo_x = 0;
                enemy->velo_y = 2;
            }
        }

    }

    if (enemy->pos_y > SCREEN_HEIGHT || enemy->pos_x > SCREEN_WIDTH || enemy->pos_x < 0){

        enemy->returning = 1;

        enemy->pos_x = enemy->start_x;
        enemy->pos_y = 0;


        calculate_velo(enemy->start_x + enemy_wiggle_time, enemy->start_y, enemy, 1, 2);
    }

    
    if (enemy->returning){


        if (abs(enemy->pos_x - enemy->start_x + enemy_wiggle_time) < 15 && abs(enemy->pos_y -enemy->start_y) < 15){

            enemy->pos_x = enemy->start_x+enemy_wiggle_time;
            enemy->pos_y = enemy->start_y;

            enemy->velo_x = 0;
            enemy->velo_y = 0;

            enemy->moving = 0;
            enemy->returning = 0;
            enemy->move_time = 0;
            enemy->turn_counter = 0;

            enemies_moving --;

        }

        else 
            calculate_velo(enemy->start_x + enemy_wiggle_time, enemy->start_y, enemy, 1, 2);

    }

    enemy->pos_x += enemy->velo_x;
    enemy->pos_y += enemy->velo_y;
}

void change_row_ends(int i, int row_num, int front){


    if(!front){

        for (int j=i-1; game_state.enemies[j].row == row_num; j--){

            if(game_state.enemies[j].active && !game_state.enemies[j].moving){

                row_backs[row_num] = j;
                break;
            }
        }
    }
    else{

        for (int j=i+1; game_state.enemies[j].row == row_num; j++){

            if(game_state.enemies[j].active && !game_state.enemies[j].moving){

                row_fronts[row_num] = j;
                break;
            }
        }
    }
}

int enemies_to_move(){

    int active2 = 0, active3 = 0, active4 = 0;
    enemy *enemy;


    for (int i = 0; i< ENEMY_COUNT; i++){

        enemy = &game_state.enemies[i];

        switch (enemy->sprite){

            case 2:
                active2 ++;
                break;

            case 3:
                active3 ++;
                break;

            case 4:
                active4 ++;
                break;
        }
    }

    int rand_enemy = rand() % (active2 + active3 + active4);

    if (rand_enemy < active2) {
        rand_enemy = 2;
    } else if (rand_enemy < active2 + active3) {
        rand_enemy =  3;
    } else {
        rand_enemy = 4;
    }

    if (round_time < 100) return 0;

    else if (round_time > 500){

        if(round_time >= 750) round_time = 0;

        return 0;
    }
    else{

        if(round_time % 100 == 0) return rand_enemy;

        else return 0;
    }

}


int enemy_movement(int rand_enemy){

    int cont, row_num, num_left = 0;
    enemy *enemy;

    if (rand_enemy != 0){

        if(rand_enemy == 4) row_num = 3 + rand() % 2;

        else if(rand_enemy == 3) row_num = 1 + rand() % 2;

        else if(rand_enemy == 2) row_num = 0;

        if (enemy_wiggle > 0) rand_enemy = row_backs[row_num];
        else rand_enemy = row_fronts[row_num];

    }

    else rand_enemy = -1;

    for (int i = 0; i < ENEMY_COUNT; i++){

        enemy = &game_state.enemies[i];

        if (enemy->active){

            num_left++;

            if(enemy->active && !enemy->moving && rand_enemy == i){

                if (enemy_wiggle > 0) change_row_ends(i, row_num, 0);
            
                else change_row_ends(i, row_num, 1);

                enemy-> velo_x = 0;
                enemy->velo_y = -4;

                enemy->moving = 1;
                enemies_moving ++;
            }

            if(enemy->moving) {

                enemy_attack(enemy);

                if (!enemy->moving){

                    if(i > row_backs[enemy->row]) row_backs[enemy->row] = i;
                    if(i < row_fronts[enemy->row]) row_fronts[enemy->row] = i;
                }

                else
                    enemy_shoot(enemy);
            }

            else
                enemy->pos_x += enemy_wiggle;

        }
        else {

            if(game_state.bullets[enemy->bul1].active|| 
                game_state.bullets[enemy->bul1].active)

                enemy_shoot(enemy);                

            else continue;
        }


        if (abs(game_state.ship.pos_x - enemy->pos_x) <= SHIP_WIDTH
            && abs(game_state.ship.pos_y - enemy->pos_y) <= SHIP_HEIGHT){

                enemy->active = 0;

                if(i == row_backs[enemy->row]) change_row_ends(i, enemy->row, 0);

                else if (i == row_fronts[enemy->row]) change_row_ends(i, enemy->row, 1);

                game_state.ship.lives -= 1;
                num_left --;
            }
    }

    return num_left;
}

void bullet_movement(int new_bullet){

    bullet *bul;
    enemy *enemy;

    for (int i = 0; i < SHIP_BULLETS; i++) {

        bul = &game_state.ship.bullets[i];

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


                    if(i == row_backs[enemy->row]) change_row_ends(i, enemy->row, 0);

                    else if (i == row_fronts[enemy->row]) change_row_ends(i, enemy->row, 1);

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

void ship_movement(){

    spaceship *ship = &game_state.ship;

    if(ship->pos_x > 0 && ship->pos_x < SCREEN_WIDTH-SHIP_WIDTH)
        ship->pos_x += ship->velo_x;

    if(ship->pos_y > 0 && ship->pos_y < SCREEN_HEIGHT-SHIP_HEIGHT)
    ship->pos_y += ship->velo_y;
}


struct libusb_device_handle *controller;

uint8_t endpoint_address;


int main(){

    spaceship *ship = &game_state.ship;
    controller_packet packet;
    int transferred, start = 0, new_bullet, prev_bullet = 0, enemies_remaining, rand_enemy;
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

        round_time++;

        enemy_wiggle_time += enemy_wiggle;
        if (abs(enemy_wiggle_time) == 80) enemy_wiggle = -enemy_wiggle;

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
            rand_enemy = enemies_to_move();
            enemies_remaining = enemy_movement(rand_enemy);

            if(ship->lives <= 0){
                printf("You lost =( \n");
                break;
            }

            if(!enemies_remaining){

                if(round_num == 3){

                    printf("You Won!");
                    break;
                }


                row_vals[0] ++;

                for(int i =1; i<5; i++){

                    row_vals[i] += round_num*2;
                }

                round_num++;

            }


            update_hardware();

            usleep(16000);
        }    
    }

}
