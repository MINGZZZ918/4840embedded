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


#define COLOR_COUNT 5

#define SHIP_INITIAL_X 300
#define SHIP_INITIAL_Y 400

#define BULLET_WIDTH 8
#define BULLET_HEIGHT 4


#define ENEMY_WIDTH 16
#define ENEMY_HEIGHT 16

#define ENEMY_SPACE 10
#define COLUMNS 22

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


#define NUM_ROWS 5
// static char row_vals[NUM_ROWS] = {0,0,0,0,1};
// static char row_vals[NUM_ROWS] = {0,4,3,2,1};
static char row_vals[NUM_ROWS] = { 2, 6, 8, 10, 10 };
static char row_sprites[NUM_ROWS] = { ENEMY1, ENEMY2,ENEMY2, ENEMY3, ENEMY3};
static int row_fronts[NUM_ROWS];
static int row_backs[NUM_ROWS];


static int kill_count = 0;

static int ship_velo = 2;

static int powerup_timer = 0;
#define EXTRA_BULLET_TIME 300;
#define EXTRA_SPEED_TIME 750;


#define EXPLOSION_TIME 10


static int blink_counter;
#define BLINK_COUNT 10
#define QUICK_BLINK_COUNT 5


static int round_frequency = 100;



static int enemy_wiggle = 1;
static int enemy_wiggle_time = 0;


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





#define TURN_TIME 70
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


static int num_enemies_moving = 0, active_ship_buls = 0, active_enemy_buls = 0;


static int round_wait_time = 0;
static long round_time = 0;
static int active1 = 0, active2 = 0, active3 = 0, round_pause, num_sent, send_per_round = 20;
#define TOTAL_ACTIVE (active1 + active2 + active3)
#define ROUND_WAIT 100
static int round_num = 1;



static gamestate game_state = {

    .ship = {.pos_x = SHIP_INITIAL_X, .pos_y = SHIP_INITIAL_Y, .velo_x = 0, .velo_y = 0, .lives = LIFE_COUNT, .num_buls = 3, .bullets = { 0 }, .active = 1},
    .background = {.red = 0x00, .green = 0x00, .blue = 0x20},
    .bullets = { 0 },
    .enemies = { 0 },
    .power_up = { 0 },
    .score = 0
};

/**
 * Update game state and send to the device
 */
void update_enemies() {
    if (ioctl(vga_ball_fd, UPDATE_ENEMIES, &game_state)) {
        perror("ioctl(UPDATE_ENEMIES) failed");
        exit(EXIT_FAILURE);
    }
}


void update_ship() {
    if (ioctl(vga_ball_fd, UPDATE_SHIP, &game_state.ship)) {
        perror("ioctl(UPDATE_SHIP) failed");
        exit(EXIT_FAILURE);
    }
}

void update_ship_bullet() {
    if (ioctl(vga_ball_fd, UPDATE_SHIP_BULLETS, &game_state.ship)) {
        perror("ioctl(UPDATE_SHIP_BULLETS) failed");
        exit(EXIT_FAILURE);
    }
}


void update_powerup() {
    if (ioctl(vga_ball_fd, UPDATE_POWERUP, &game_state.power_up)) {
        perror("ioctl(UPDATE_POWERUP) failed");
        exit(EXIT_FAILURE);
    }
}



void apply_powerup(powerup *power_up){

    spaceship *ship = &game_state.ship;

    switch (power_up->sprite){

        case EXTRA_LIFE:

            ship->lives++;

            // draw an extra ship life
            break;

        case SHIP_SPEED:

            ship_velo = 3;
            powerup_timer = EXTRA_SPEED_TIME;
            break;

        case EXTRA_BULLETS:

            ship->num_buls = 3;
            powerup_timer = EXTRA_BULLET_TIME;
            break;
    }
}

void active_powerup(){

    powerup *power_up = &game_state.power_up;

    if (--powerup_timer < 0){

        game_state.ship.num_buls = 1;
        ship_velo = 2;
    }

    else if (powerup_timer == 0)
        power_up->active = 0;

    else if(powerup_timer > 50 && powerup_timer < 100){

        if (blink_counter){
            power_up->active = !power_up->active;
            blink_counter --;
        }
        else{
            blink_counter = BLINK_COUNT;
        }
    }
    else if (powerup_timer > 0 && powerup_timer < 50){
        
        if (blink_counter){
            power_up->active = !power_up->active;
            blink_counter --;
        }
        else{
            blink_counter = QUICK_BLINK_COUNT;
        }
    }
}

void move_powerup(){

    powerup *power_up = &game_state.power_up;
    spaceship *ship = &game_state.ship;

    if (power_up->active){

        power_up->pos_y += 1;

        if (ship->active && 
            abs(ship->pos_x - power_up->pos_x) <= SHIP_WIDTH &&
            abs(ship->pos_y - power_up->pos_y) <= SHIP_HEIGHT){

            apply_powerup(power_up);

            power_up->pos_x = 400;
            power_up->pos_y = SCREEN_HEIGHT-SHIP_HEIGHT;
            kill_count = 0;
        }

        if (power_up->pos_y >= SCREEN_HEIGHT){

            power_up->active=0;
            kill_count = 10;
        }

    }

}

void drop_powerup(enemy *enemy){

    powerup *power_up = &game_state.power_up;
    int i = rand() % 3;

    if (game_state.ship.lives == LIFE_COUNT && i == 2)
        while (i == 2) i = rand() % 3;

    power_up->pos_x = enemy->pos_x;

    power_up->pos_y = 200;
    power_up->active = 1;

    switch (i){
        case 0:
            power_up->sprite = SHIP_SPEED;

            break;

        case 1:
            power_up->sprite = EXTRA_BULLETS;
            break;

        case 2:
            power_up->sprite = EXTRA_LIFE;
            break;

        default:
            break;
    }
}



void change_active_amount(char enemy_sprite){

    switch(enemy_sprite){

        case ENEMY1:
            active1 --;
            break;

        case ENEMY2:
            active2 --;
            break;

        case ENEMY3:
            active3 --;
            break;
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


void change_row_ends(int cur_end, int row_num, int front){


    if(!front){

        for (int i=cur_end-1; game_state.enemies[i].row == row_num; i--){

            if(game_state.enemies[i].active && !game_state.enemies[i].moving){

                row_backs[row_num] = i;
                break;
            }
        }
    }
    else{

        for (int i=cur_end+1; game_state.enemies[i].row == row_num; i++){

            if(game_state.enemies[i].active && !game_state.enemies[i].moving){

                row_fronts[row_num] = i;
                break;
            }
        }
    }
}



bool aquire_bullet(enemy *enemy){

    bullet *bul;
    
    for (int i = 0; i<MAX_BULLETS; i++){

        bul = &game_state.bullets[i];

        if(!bul->active && game_state.ship.active){

            bul->active = 1;

            bul->pos_x = enemy->pos_x;
            bul->pos_y = enemy->pos_y+(ENEMY_HEIGHT);

            bul->enemy = enemy->position;
            
            enemy->bul = i;

            active_enemy_buls ++;

            return 1;
        }
    }

    return 0;


}


void enemy_shoot(enemy *enemy){

    spaceship *ship = &game_state.ship;

    bool aquired;

    if (ship->active && enemy->bul_cooldown <= 0 && 
        enemy->turn_counter >= TURN_TIME && !enemy->returning){

        if (enemy->sprite == ENEMY2){

            if (abs(ship->pos_x - enemy->pos_x) <= 80
                    && abs(ship->pos_y - enemy->pos_y) <= 150
                    && ship->pos_y - 30 > enemy->pos_y){

                if (enemy->bul == -1){

                    if((aquired = aquire_bullet(enemy))){

                        enemy->bul_cooldown = ENEMY3_BULLET_COOLDOWN;
                        game_state.bullets[enemy->bul].velo_y = 3;
                        game_state.bullets[enemy->bul].velo_x = 0;

                    }
                }
            }
        }

        else if(enemy->sprite == ENEMY3){

            if (abs(ship->pos_x - enemy->pos_x) <= 150
                && abs(ship->pos_y - enemy->pos_y <= 200
                && ship->pos_y - 60 > enemy->pos_y)){

                if (enemy->bul == -1){

                    if((aquired = aquire_bullet(enemy))){

                        enemy->bul_cooldown = ENEMY4_BULLET_COOLDOWN;
                        calculate_velo(ship->pos_x, ship->pos_y, &game_state.bullets[enemy->bul], 0, 4);

                    }
                }
            }
        }
    }

    else if(enemy->turn_counter <= TURN_TIME)
        enemy->bul_cooldown --;

}


void enemy_return (enemy *enemy){

    int position;


    if (enemy->pos_y > SCREEN_HEIGHT || enemy->pos_x > SCREEN_WIDTH || enemy->pos_x < 0){

        enemy->returning = 1;

        enemy->pos_x = enemy->start_x;
        enemy->pos_y = 0;

        calculate_velo(enemy->start_x + enemy_wiggle_time, enemy->start_y, enemy, 1, 2);
    }

    
    if (enemy->returning){

        if (abs(enemy->pos_x - enemy->start_x + enemy_wiggle_time) < 25 && abs(enemy->pos_y -enemy->start_y) < 25){

            enemy->pos_x = enemy->start_x+enemy_wiggle_time;
            enemy->pos_y = enemy->start_y;

            enemy->velo_x = 0;
            enemy->velo_y = 0;

            enemy->moving = 0;
            enemy->returning = 0;
            enemy->move_time = 0;
            enemy->turn_counter = 0;

            num_enemies_moving --;

        }

        else 
            calculate_velo(enemy->start_x + enemy_wiggle_time, enemy->start_y, enemy, 1, 2);

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


        if(enemy->sprite == ENEMY1){

            enemy->velo_x = (enemy->pos_x < SCREEN_WIDTH / 2) ? 2 : -2;
            enemy->velo_y = 1;
        }

        else if(enemy->sprite == ENEMY2){

            enemy->velo_x = (enemy->pos_x < SCREEN_WIDTH / 2) ? 4 : -4;
            enemy->velo_y = 2;
        }
        else {

            calculate_velo(ship->pos_x, ship->pos_y, enemy, 1, 3);
        }

    }

}


void enemy_attack(enemy *enemy){


    spaceship *ship = &game_state.ship;
    int cont;


    enemy->pos_x += enemy->velo_x;
    enemy->pos_y += enemy->velo_y;


    if (enemy->turn_counter < TURN_TIME)
        turn(enemy);

    else{

        if (enemy->sprite == ENEMY1){

            if (!ship->active){

                enemy->velo_x = 0;
                enemy->velo_y = 4;
            }

            else if(++enemy->move_time < 250)
                calculate_velo(ship->pos_x, ship->pos_y, enemy, 1, 3);
            else{

                enemy->velo_x = 0;
                enemy->velo_y = 2;
            }
        }

        else if (enemy->sprite == ENEMY2){


            if (enemy->pos_y+30 >= ship->pos_y || !ship ->active) {

                enemy->velo_x = (enemy->pos_x > ship->pos_x) ? 1 : -1;
                enemy->velo_y = 4;

            }

            else if (enemy->move_time == 0){

                if (enemy->start_x < SCREEN_WIDTH/2 && enemy->pos_x - ship->pos_x > 10 && ship->pos_y - enemy->pos_y < 150){
                    
                    calculate_velo(ship->pos_x, ship->pos_y, enemy, 1, 2);
                    enemy->move_time++;
                }

                else if (ship->pos_x - enemy->pos_x > 10 && ship->pos_y - enemy->pos_y < 150){

                    calculate_velo(ship->pos_x, ship->pos_y, enemy, 1, 2);
                    enemy->move_time++;
                }
            }

            else{

                if(enemy->move_time < 75){

                    if (enemy->start_x < SCREEN_WIDTH/2)
                        // calculate_velo(ship->pos_x -200, ship->pos_y, enemy, 1, 2);
                        enemy->velo_x = -2;

                    else
                        // calculate_velo(ship->pos_x +200, ship->pos_y, enemy, 1, 2);
                        enemy->velo_x = 2;
                }

                else{
                    
                    if (enemy->start_x < SCREEN_WIDTH/2)
                        // calculate_velo(ship->pos_x +200, ship->pos_y, enemy, 1, 2);
                        enemy->velo_x = 2;
                    else
                        // calculate_velo(ship->pos_x -200, ship->pos_y, enemy, 1, 2);
                    enemy->velo_x = -2;
                }

                if(++ enemy->move_time > 150)
                    calculate_velo(ship->pos_x, ship->pos_y, enemy, 1, 2);
            }
        }

        else{

            if (!ship->active){

                enemy->velo_x = (enemy->pos_x > ship->pos_x) ? 1 : -1;
                enemy->velo_y = 4;
            }

            else if(++enemy->move_time == 150){

                cont = rand() % 4;

                if(!cont)
                    enemy->velo_x = -enemy->velo_x;

                else
                    enemy->move_time --;
            }

            else if(enemy->move_time == 250){

                enemy->velo_x = 0;
                enemy->velo_y = 2;
            }
            else if (enemy->pos_y > ship->pos_y){

                enemy->velo_x = (enemy->pos_x > ship->pos_x) ? 1 : -1;
                enemy->velo_y = 2;
            }
        }

    }

    enemy_return(enemy);

}


void enemy_explosion(){

    enemy *enemy;

    for(int i = 0; i<ENEMY_COUNT; i++){

        enemy = &game_state.enemies[i];

        if(enemy->explosion_timer == 1){

            memset(enemy, 0, sizeof(*enemy));
        }

        else if(enemy->explosion_timer < EXPLOSION_TIME/2 && enemy->explosion_timer){
            enemy->sprite = SHIP_EXPLOSION2;
            enemy->explosion_timer --;
        }

        else if (enemy->explosion_timer){
            enemy->velo_x = 0;
            enemy->velo_y = 0;
            enemy->sprite = SHIP_EXPLOSION1;

            enemy->explosion_timer --;

        }
    }
}


void ship_explosion(){

    spaceship *ship = &game_state.ship;

    if(ship->explosion_timer == 1){
        ship->active = 0;
        ship->explosion_timer = 0;
        ship->sprite = SHIP;
    }
    else if(ship->explosion_timer < EXPLOSION_TIME/2 && ship->explosion_timer){
        ship->sprite = SHIP_EXPLOSION2;
        ship->explosion_timer --;
    }
    else if (ship->explosion_timer){


        ship->sprite = SHIP_EXPLOSION1;
        ship->explosion_timer --;

    }
}



int enemy_movement(int rand_enemy){

    int cont, row_num, num_left = 0;
    enemy *enemy;
    spaceship *ship = &game_state.ship;

    if (rand_enemy != -1){

        switch(rand_enemy){

            case ENEMY1:
                row_num = 0;
                break;

            case ENEMY2:
                row_num = 1 + rand() % 2;
                break;

            case ENEMY3:
                row_num = 3 + rand() % 2;
                break;
        }

        if (enemy_wiggle > 0) rand_enemy = row_fronts[row_num];
        else rand_enemy = row_backs[row_num];

    }

    for (int i = 0; i < ENEMY_COUNT; i++){

        enemy = &game_state.enemies[i];

        if (enemy->active && !enemy->explosion_timer){

            num_left++;

            if(!enemy->moving && rand_enemy == i){

                if (enemy_wiggle > 0) change_row_ends(i, row_num, 1);
            
                else change_row_ends(i, row_num, 0);

                enemy->velo_x = (enemy->start_x < SCREEN_WIDTH/2) ? -1 : 1;
                enemy->velo_y = -4;

                enemy->moving = 1;
                num_enemies_moving ++;
            }

            if(enemy->moving) {

                enemy_attack(enemy);

                if (!enemy->moving){

                    if(i > row_backs[enemy->row] || 
                        !game_state.enemies[row_backs[enemy->row]].active)
                        
                        row_backs[enemy->row] = i;


                    if(i < row_fronts[enemy->row] || 
                        !game_state.enemies[row_fronts[enemy->row]].active)
                    
                        row_fronts[enemy->row] = i;
                }

                else
                    enemy_shoot(enemy);
            }

            else
                enemy->pos_x += enemy_wiggle;


            if (ship->active && !ship->explosion_timer &&
                abs(ship->pos_x - enemy->pos_x) <= SHIP_WIDTH
                && abs(ship->pos_y - enemy->pos_y) <= SHIP_HEIGHT){

                enemy->active = 0;

                if(i == row_backs[enemy->row]) change_row_ends(i, enemy->row, 0);

                else if (i == row_fronts[enemy->row]) change_row_ends(i, enemy->row, 1);

                change_active_amount(enemy->sprite);

                if(enemy->moving) num_enemies_moving --;

                memset(enemy, 0, sizeof(*enemy)); 

                ship->lives --;
                ship->explosion_timer = EXPLOSION_TIME;

                round_wait_time = ROUND_WAIT;
                num_left --;


            }
        }
    }
    return num_left;
}

void move_enemy_bul(){

    spaceship *ship = &game_state.ship;
    bullet *bul;

    for (int i = 0; i<MAX_BULLETS; i++){

        bul = &game_state.bullets[i];

        if(!bul->active) continue;

        bul->pos_x += bul->velo_x;
        bul->pos_y += bul->velo_y;


        if (ship->active && !ship->explosion_timer &&
            abs(ship->pos_x - bul->pos_x ) <= SHIP_WIDTH &&
            abs(ship->pos_y - bul->pos_y ) <= SHIP_HEIGHT){


            game_state.enemies[bul->enemy].bul = -1;
            
            bul->active = 0;
            bul->enemy = -1;

            active_enemy_buls --;

            ship->lives --;
            ship->explosion_timer = EXPLOSION_TIME;

            round_wait_time = ROUND_WAIT;

        }

        if (bul->pos_y >= SCREEN_HEIGHT || bul->pos_x >= SCREEN_WIDTH || bul->pos_x < 0){

            game_state.enemies[bul->enemy].bul = -1;

            bul->active = 0;
            bul->enemy = -1;

            active_enemy_buls --;
        }
    } 
}



void bullet_colision(bullet *bul){

    enemy *enemy;

    for (int i = 0; i<ENEMY_COUNT; i++){

        enemy = &game_state.enemies[i];

        if (enemy->explosion_timer) continue;

        if (enemy->active && 
            abs(enemy->pos_x - bul->pos_x) <= ENEMY_WIDTH &&
            abs(enemy->pos_y - bul->pos_y) <= ENEMY_HEIGHT){

            if(i == row_backs[enemy->row]) change_row_ends(i, enemy->row, 0);

            else if (i == row_fronts[enemy->row]) change_row_ends(i, enemy->row, 1);

            change_active_amount(enemy->sprite);

            bul->active = 0;

            active_ship_buls --;

            if (++ kill_count >= 15 && !game_state.power_up.active &&
                game_state.ship.active && !game_state.ship.explosion_timer) 
                    drop_powerup(enemy);

            if(enemy->moving) num_enemies_moving --;

            enemy->explosion_timer = EXPLOSION_TIME;

            game_state.score++;

            break;
        }
    }
}

void bullet_movement(int new_bullet){

    bullet *bul;
    int num_active = 0;

    for(int i = 0; i< SHIP_BULLETS; i++) 
        if(game_state.ship.bullets[i].active) num_active++;

    for (int i = 0; i < SHIP_BULLETS; i++) {

        bul = &game_state.ship.bullets[i];

        if (bul->active){

            bul->pos_y += bul->velo_y;

            if (bul->pos_y <= 5){

                bul->active = 0;
                active_ship_buls --;
                continue;
            }

            bullet_colision(bul);
        }

        else if (!bul->active && new_bullet && num_active < game_state.ship.num_buls) {
            bul->active = 1;
            bul->pos_x = game_state.ship.pos_x;
            bul->pos_y = game_state.ship.pos_y-(SHIP_HEIGHT);
            bul->velo_y = -3;
            new_bullet = 0;

            active_ship_buls ++;
        }
    }
}



void ship_movement(){

    spaceship *ship = &game_state.ship;

    if(ship->velo_x > 0 && ship->pos_x < SCREEN_WIDTH-SHIP_WIDTH-5)
        ship->pos_x += ship->velo_x;

    else if(ship->velo_x < 0 && ship->pos_x > 5)
        ship->pos_x += ship->velo_x;


    if (ship->velo_y > 0 && ship->pos_y < SCREEN_HEIGHT-SHIP_HEIGHT*2-5)
        ship->pos_y += ship->velo_y;

    else if (ship->velo_y < 0 && ship->pos_y > 5)
        ship->pos_y += ship->velo_y;
    
}

// taking too long to move
// after so long I can have liek 5 go at the same time just remove %
int enemies_to_move(){

    enemy *enemy;
    int rand_enemy;


    if (TOTAL_ACTIVE != 0){
        
        rand_enemy = rand() % TOTAL_ACTIVE;

        if (rand_enemy < active1)
            rand_enemy = ENEMY1;
        
        else if (rand_enemy < active1 + active2)
            rand_enemy =  ENEMY2;
        
        else 
            rand_enemy = ENEMY3;

        if (num_sent == send_per_round){

            if (!num_enemies_moving){

                num_sent = 0;
                round_pause = ROUND_WAIT/2;
            }
        }
        else if (num_sent > send_per_round/4 && num_sent <= send_per_round/4 +3){

            num_sent ++;
            return rand_enemy;
        }
        else if (num_sent > send_per_round*3/4 && num_sent <= send_per_round*3/4 +3){

            num_sent ++;
            return rand_enemy;
        }
        else{

            if(round_time % round_frequency == 0) {
                
                printf("%ld \n", round_time);

                num_sent ++;
                return rand_enemy;
            }

            else return -1;
        }
    }

    return -1;

}

void init_round_state() {

    int space, row = 0, enemy_count;

    enemy *enemy;

    enemy_count = row_vals[row];

    space = COLUMNS - row_vals[row];

    row_fronts[row] = 0;

    for (int i = 0, j=0; i < ENEMY_COUNT; i++, j++) {

        enemy = &game_state.enemies[i];

        memset(enemy, 0, sizeof(*enemy));

        while (i >= enemy_count && row < 5){

            row_backs[row] = i-1;

            row++;
            
            j = 0;

            space = COLUMNS - row_vals[row];
            enemy_count += row_vals[row];

            row_fronts[row] = i;
        }

        if (row < 5){
        
            enemy->pos_x = enemy->start_x = 50 + ((ENEMY_WIDTH + ENEMY_SPACE) * (space / 2)) \
                + j * (ENEMY_WIDTH + ENEMY_SPACE);
                                    
            enemy->pos_y = enemy->start_y = 60 + 30 *(row+1);
            enemy->sprite = row_sprites[row];
            enemy->position = i;
            // enemy->active = 1;
            enemy->bul = -1;
            enemy->row = row;
            enemy->col = (space/2) + j;

            switch(row_sprites[row]){

                case ENEMY1:
                    active1 ++;
                    break;

                case ENEMY2:
                    active2 ++;
                    break;

                case ENEMY3:
                    active3 ++;
                    break;
            }
        }
        else
            enemy->col = -1;
    }
}

struct libusb_device_handle *controller;

uint8_t endpoint_address;


int main(){

    spaceship *ship = &game_state.ship;
    controller_packet packet;
    int transferred, start = 0, new_bullet, prev_bullet = 0, enemies_remaining, rand_enemy;
    int col_active = 0, active_buls = 0, active_enemies = 0;
    int bumpers = 0, buttons = 0;

    srand(time(NULL));


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

    init_round_state();
    update_ship();

    usleep(16000);

    for (int i =0; i<COLUMNS; i++){
        for(int j=0; j<ENEMY_COUNT; j++)
            if(game_state.enemies[j].col == i) game_state.enemies[j].active = 1;

        update_enemies();
        usleep(16000);
    }

    for (;;){

        round_time++;
        active_buls = 0;
        active_enemies = 0;

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
                        ship->velo_x = -ship_velo;

                    // printf("%d, %d \n", ship->pos_x, ship->pos_y);
                    break;
                    
                case RIGHT_ARROW:
                    if(ship->pos_x < SCREEN_WIDTH-SHIP_WIDTH)
                        ship->velo_x = ship_velo;

                    // printf("%d, %d \n", ship->pos_x, ship->pos_y);
                    break;

                default:
                    ship->velo_x = 0;
                    break;
            }

            switch (packet.ud_arrows) {
                case UP_ARROW:
                    if (ship->pos_y < SCREEN_HEIGHT - 5)
                        ship->velo_y = -ship_velo;

                    // printf("%d, %d\n", ship->pos_x, ship->pos_y);
                    break;
                    
                case DOWN_ARROW:
                    if (ship->pos_y > 0+SHIP_HEIGHT)
                        ship->velo_y = ship_velo;

                    // printf("%d, %d \n", ship->pos_x, ship->pos_y);
                    break;

                default:
                    ship->velo_y = 0;
                    break;
            }

            switch (packet.buttons) {                
                case Y_BUTTON:
                    if (!prev_bullet ){
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
                    if (!prev_bullet){
                        new_bullet = 1; // do not allow them to hold the button to shoot
                        prev_bullet = 1;
                    }

                    bumpers = 1;

                    break;
                    
                case RIGHT_BUMPER:
                    if (!prev_bullet){
                        new_bullet = 1; // do not allow them to hold the button to shoot
                        prev_bullet = 1;
                    }

                    bumpers = 1;

                    break;

                case LR_BUMPER:
                    if (!prev_bullet ){
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

            if(ship->active && !ship->explosion_timer) ship_movement();

            move_powerup();
            enemy_explosion();
            ship_explosion();

            if (!round_wait_time){ // ship is alive and round is playing

                active_powerup();

                if(ship->active) bullet_movement(new_bullet); 

                rand_enemy = enemies_to_move();
                enemies_remaining = enemy_movement(rand_enemy);
                move_enemy_bul();

            }

            else if(round_wait_time == 1){

                if(!ship->active){

                    ship->active = 1;
                    ship->pos_x = SHIP_INITIAL_X;
                    ship->pos_y = SHIP_INITIAL_Y;
                    round_wait_time = 0;
                    round_time = 0;

                    num_sent = 0;

                    powerup_timer = 0;
                    kill_count /= 2;
                } 

                else{

                    for(int j=0; j<ENEMY_COUNT; j++)
                        if(game_state.enemies[j].col == col_active) game_state.enemies[j].active = 1;

                    if (++col_active == COLUMNS) round_wait_time = 0;
                }
            }

            else{

                printf("%d, %d, %d \n", active_ship_buls, active_enemy_buls, num_enemies_moving);

                if(!active_ship_buls && !active_enemy_buls && !num_enemies_moving)
                    round_wait_time --;
                        
                if (round_wait_time > 10) round_wait_time --;

                enemy_movement(-1);
                move_enemy_bul();
                bullet_movement(0);

            }

            update_ship();
            update_enemies();
            update_powerup();
            update_ship_bullet();

            if(ship->lives <= 0){
                printf("You lost =( \n");

                memset(&game_state, 0, sizeof(gamestate));

                update_ship();
                update_enemies();
                update_powerup();
                update_ship_bullet();


                break;
            }

            if(!enemies_remaining){

                if(round_num == 3){

                    printf("You Won!");

                    memset(&game_state, 0, sizeof(gamestate));

                    update_ship();
                    update_enemies();
                    update_powerup();
                    update_ship_bullet();

                    break;
                }

                if(!active_enemy_buls){

                    enemy_wiggle_time = 0;
                    enemy_wiggle = 1;

                    round_wait_time = ROUND_WAIT;
                    col_active = 0;

                    round_time = 0;
                    num_sent = 0;

                    round_frequency -=25;

                    send_per_round += send_per_round/4;

                    active1 = active2 = active3 = 0;

                    row_vals[0] ++;

                    for(int i =1; i<5; i++){

                        row_vals[i] += round_num*2;
                    }

                    init_round_state();

                    enemies_remaining = 1;
                    round_num++;
                }

            }

            usleep(16000);
        }    
    }

}
