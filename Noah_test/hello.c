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
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include "vga_ball.h"
#include "contoller.h"



#define SHIP_INITIAL_X 200
#define SHIP_INITIAL_Y 240

#define SHIP_WIDTH 100
#define SHIP_HEIGHT 100

#define ENEMY_WIDTH 100
#define ENEMY_HEIGHT 100

#define MAX_BULLETS 5
#define ENEMY_COUNT 40
#define LIFE_COUNT 5

#define INPUT_QUEUE_SIZE 32

#define COLOR_COUNT 5

#define NUM_ENEMIES 5 // how many different sprites we have

/* File descriptor for the VGA ball device */
static int vga_ball_fd;

static const char filename[] = "/dev/vga_ball";

static gamestate game_state = {

    .ship = {.pos_x = SHIP_INITIAL_X, .pos_y = SHIP_INITIAL_Y, .velo_x = 0, .velo_y = 0, .lives = LIFE_COUNT},
    .queue = {.head = 0, .tail = 0, .lock = PTHREAD_MUTEX_INITIALIZER},
    .background = {.red = 0x00, .green = 0x00, .blue = 0x20}
};

/* Array of background colors to cycle through */
static const vga_ball_color_t colors[] = {
    { 0x00, 0x00, 0x10 },  // Very dark blue
    { 0x00, 0x00, 0x20 },  // Dark blue
    { 0x10, 0x10, 0x30 },  // Navy blue
    { 0x00, 0x00, 0x40 },  // Medium blue
    { 0x20, 0x20, 0x40 }   // Blue-purple
};

/**
 * Initialize game state
 */
void init_game_state() {

    for (int i = 0; i < ENEMY_COUNT; i++) {

        if(i < MAX_BULLETS){

            game_state.bullets[i].pos_x = 0;
            game_state.bullets[i].pos_y = 0;
            game_state.bullets[i].velo_y = 0;
            game_state.bullets[i].active = 0;
        }

        game_state.enemies[i].pos_x = i * ENEMY_WIDTH;
        game_state.enemies[i].pos_y = i * ENEMY_HEIGHT;
        game_state.enemies[i].velo_x = 1;
        game_state.enemies[i].velo_y = 0;
        game_state.enemies[i].sprite = i % NUM_ENEMIES;
        game_state.enemies[i].moving = 1;
        game_state.enemies[i].active = 1;

        game_state.enemies[i].bul.pos_x = 0;
        game_state.enemies[i].bul. pos_y = 0;
        game_state.enemies[i].bul.velo_y = 0;
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


void enqueue(input_queue *queue, event *events, int count) {

    int next_tail;

    pthread_mutex_lock(&queue->lock);

    for(int i = 0; i < count; i++){

        next_tail = (queue->tail + 1) % INPUT_QUEUE_SIZE;

        if (next_tail != queue->head) {  // Only insert if queue is not full

            queue->events[queue->tail] = events[i];
            queue->tail = next_tail;
        }
    }
    pthread_mutex_unlock(&queue->lock);
}

void dequeue(input_queue *queue, event *events, int count) {

    int num_events;

    pthread_mutex_lock(&queue->lock);

    for(int i = 0; i < count; i++){

        if (queue->head != queue->tail) {
            events[i] = queue->events[queue->head];
            queue->head = (queue->head + 1) % INPUT_QUEUE_SIZE;
            num_events++;
        }
        else events[i] = QUEUE_EMPTY;
    }
    pthread_mutex_unlock(&queue->lock);
}

pthread_t game_thread;
void *game_logic(void *);

struct libusb_device_handle *contoller;

uint8_t endpoint_address;


int main(){

    int start = 0;
    event input_events[4];
    controller_packet packet;
    int transferred;


    // /* Initialize random number generator */
    // srand(time(NULL));

    /* Open the device file */
    if ((vga_ball_fd = open(filename, O_RDWR)) == -1) {
        fprintf(stderr, "Could not open %s\n", filename);
        return EXIT_FAILURE;
    }

    while (start == 0){
        // recieve packets 

        libusb_interrupt_transfer(controller, endpoint_address,
            (unsigned char *) &packet, sizeof(packet), &transferred, 0);

        if(packet.buttons == 0x2f) start = 1;
    }

    // init_game_state();
    // update_hardware();

    /* Start the game thread */
    // pthread_create(&game_thread, NULL, game_logic, NULL);

    for (;;){

        if (game_state.ship.lives == 0) break;

        libusb_interrupt_transfer(controller, endpoint_address,
            (unsigned char *) &packet, sizeof(packet), &transferred, 0);
            
        if (transferred == sizeof(packet)) {

            switch (packet.lr_arrows) {
                case 0x00:
                    input_events[0] = LEFT;
                    printf("left \n");
                    break;
                    
                case 0xff:
                    input_events[0] = RIGHT;
                    printf("right \n");
                    break;

                default:
                    input_events[0] = LR_NONE;
                    printf("none \n");
                    break;
            }


            switch (packet.ud_arrows) {
                case 0x00:
                    input_events[1] = UP;
                    printf("up \n");
                    break;
                    
                case 0xff:
                    input_events[1] = DOWN;
                    printf("down \n");
                    break;

                default:
                    input_events[1] = UD_NONE;
                    printf("none \n");
                    break;
            }

            switch (packet.buttons) {
                case 0x2f:
                    input_events[2] = BUTTON_A;
                    printf("aaaaaaa \n");
                    break;
                    
                case 0x4f:
                    input_events[2] = BUTTON_B;
                    break;

                case 0x1f:
                    input_events[2] = BUTTON_X;
                    break;
                
                case 0x8f:
                    input_events[2] = BUTTON_Y;
                    break;

                default:
                    input_events[2] = BUTTON_NONE;
                    break;
            }

            switch (packet.bumpers) {
                case 0x01:
                    input_events[3] = BUMPER_L;
                    break;
                    
                case 0x02:
                    input_events[3] = BUMPER_R;
                    break;

                case 0x10:
                    input_events[3] = SELECT;
                    break;
                
                case 0x20:
                    input_events[3] = START;
                    break;

                default:
                    input_events[3] = BUTTON_NONE;
                    break;
            }
            // enqueue(&game_state.queue, input_events, 4);
        }    
    }
}


void ship_movement(){

    spaceship *ship = game_state.ship;

    ship->pos_x += ship->velo_x;
    ship->pos_y += ship->velo_y;
}

void bullet_movement(int new_bullets){ // probably going to have to pass the current level to determine bullet velo

    bullet *bul;
    enemy *enemy;

    for (int i = 0; i < MAX_BULLETS; i++) {

        bul = game_state.bullets[i];

        if (bul->active){

            bul->pos_y += bul->velo_y;
            if (bul->pos_y == 0){ // top of screen

                bul->active = 0;
                continue;
            }

            for (int j = 0; j<ENEMY_COUNT; j++){ // checking to see if we hit an enemy

                enemy = game_state.enemies[j];

                if (enemy->active && abs(enemy->pos_x - bul->pos_x) <= ENEMY_WIDTH
                    && abs(enemy->pos_y - bul->pos_y) <= ENEMY_HEIGHT){

                    enemy->active = 0;
                    bul->active = 0;
                    break;
                }
            }
        }
        else if (!bul->active && new_bullets) {
            bul->active = 1;
            bul->pos_x = ship->pos_x+1; // make it start in the middle of the ship
            bul->pos_y = ship->pos_y-1; // make it start above the ship
            bul->velo_y = -1; 
            new_bullets--;

            // may have issue where two bullets are on top of each other here
            // can change that by multiplying the starting position by new_bullets
        }
    }
}

int enemy_movement(int enemy_dir){

    enemy *enemy;
    int num_left = 0;

    for (int i = 0; i < ENEMY_COUNT; i++){

        enemy = game_state.enemies[i];

        if (enemy->active){

            num_left ++;

            if (enemy->moving){
                enemy->pos_x += enemy->velo_x;
                enemy->pos_y += enemy->velo_y;
            }
            else{
                // check if it should move towards the ship

                //else need to move the enemy side to side
                if(enemy_dir = 0) enemy->velo_x *= -1;
                enemy->pos_x += enemy->velo_x;
            }
        }

        // important!!! compare to whichever has the larger size
        if (enemy->active && abs(ship->pos_x - enemy->pos_x) <= SHIP_WIDTH
        && abs(ship->pos_y - enemy->pos_y) <= SHIP_HEIGHT){

            enemy->active = 0;
            ship->lives-=1;
            num_left --;
        }
    }
    return num_left;
}


void *game_logic(void *ingored){

    spaceship *ship = &game_state.ship;
    int new_bullets, enemy_dir = 0;
    event input_events[4];
    int count;

    for(;;){

        new_bullets = 0;

        if (++enemy_dir > 5) enemy_dir = 0;

        // need to implement potential logic to have the velocities speed up depending on a powerup etc
        while((count = dequeue(&game_state.queue, input_events, 4)) > 0){

            switch (input_events[0]) {
                case LEFT:
                    ship->velo_x = -1;
                    break;
                    
                case RIGHT:
                    ship->velo_x = -1;
                    break;

                case LR_NONE:
                    ship->velo_x = 0;
                    break;

                default:
                    break;
            }

            switch (input_events[1]) {
                case UP:
                    ship->velo_y = 1;
                    break;
                    
                case DOWN:
                    ship->velo_y = -1;
                    break;

                case UD_NONE:
                    ship->velo_y = 0;
                    break;

                default:
                    break;
            }

            switch (input_events[2]) {
                case BUTTON_A:
                    new_bullets++;
                    break;
                    
                case BUTTON_B:
                    break;

                case BUTTON_X:
                    break;
                
                case BUTTON_Y:
                    break;

                case BUTTON_NONE:
                    break;

                default:
                    break;
            }

            switch (input_events[3]) {
                case BUMPER_L:
                    break;
                    
                case BUMPER_R:
                    break;

                case SELECT:
                    break;
                
                case START:
                    break;

                case BUMPER_NONE:
                    break;

                default:
                    break;
            }
        }

        ship_movement();
        bullet_movement(new_bullets);
        enemies_remaining = enemy_movement(enemy_dir);

        update_hardware(game_state);
    
        if (ship -> lives <= 0){
            printf("You Lost!");
            break;
        }

        if (enemies_remaining == 0){
            printf("You Won!");
            break;
        }

        usleep(16000);
    }

    return NULL;
}

// while (!vga_in_vblank()) {
//     // wait
// }




