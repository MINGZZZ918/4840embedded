/*
 * CSEE 4840 Lab 2 for 2024
 *
 * Jingyi Lai(jl6932) Hiroki Endo(he2305)
 *

void release_key_hold(void);  // function prototype


 */

#include "fbputchar.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "usbkeyboard.h"
#include <pthread.h>
#include "usb_hid_keys.h"
#include <errno.h>
#include <string.h>
#include <ifaddrs.h>

/* Adjustable long-press repetition interval (milliseconds) */
#define KEY_HOLD_INTERVAL 80

/*
 * Adjust SERVER_HOST to the correct chat server address
 * (not the main focus here).
 */
#define SERVER_HOST "128.59.19.114"
#define SERVER_PORT 42000

#define BUFFER_SIZE 128

int caps_lock = 0; /* 0 or 1 to reflect CapsLock state */

int sockfd; /* TCP socket file descriptor */
char keys[6];           /* Current keys pressed */
char pressed_keys[6];   /* Old (previous) keys pressed */
char msgbuffer[MESSAGE_SIZE+1];  /* The message to be sent. */
char myAddr[BUFFER_SIZE];

/* 
 * text_pos: For upper chat area (received messages).
 */
struct position text_pos = {
  .row = 1,
  .col = 0,
  .buf_idx = 0,
};

/*
 * msg_pos: Cursor position for the input box (where you type).
 */
struct position msg_pos = {
  .row = MSG_START_ROW+1,
  .col = 0,
  .buf_idx = 0,
};

/* Data structure to store multi-page chat history. */
struct msg_history text_box_his = {
  .count = 0,
  .curr = -1
};

struct libusb_device_handle *keyboard;
uint8_t endpoint_address;

/* Variables used for "long-press" repeating. */
uint8_t hold_key;         // The key currently being repeated
uint8_t hold_modifiers;   // Modifiers (e.g. SHIFT) for that key

pthread_t network_thread;
pthread_t network_thread_send;

/* The dedicated thread for repeating a held key. */
pthread_t keyhold_thread;

/* Forward declarations */
void *network_thread_f(void *);
void *network_thread_s(void *);
void *keyhold_thread_f(void *);

/* Key input handlers */
int handle_keyboard_input(struct usb_keyboard_packet *packet);
int handle_key_press(char keycode, char modifiers);
int handle_key_release(char keycode, char modifiers);

/* Helper to draw characters, manage cursor, etc. */
void print_char(char key, struct position *pos, char *msg_buf);
void debug_save_previous_page(char *page, int lineLen, int lines);
void update_cursor(struct position *new_pos);
void cursor_left(struct position *pos);
void cursor_right(struct position *pos);
void async_send_message(char *msg);
int message_type(char *message);

/* Possibly used for concurrency (if needed) */
pthread_mutex_t lock;  

/* Functions for text box/pagination */
void reload_txt_box();
void page_left();
void page_right();

/* Functions for editing message content */
void handle_back_space(char keycode, char modifiers, struct position *new_pos);
void destroy_pages(struct msg_history *his);
char *alloc_new_text_page(struct msg_history *his);

int main()
{
  int err, col;

  struct sockaddr_in serv_addr;

  struct usb_keyboard_packet packet;
  int transferred;

  memset(myAddr, 0, BUFFER_SIZE);
  memset(msgbuffer, 0, BUFFER_SIZE+1);

  keyhold_thread = 0;
  hold_key = 0;

  /* Initialize the framebuffer (for on-screen text) */
  if ((err = fbopen()) != 0) {
    fprintf(stderr, "Error: Could not open framebuffer: %d\n", err);
    exit(1);
  }

  /* Clear the screen */
  clear_screen();
  
  /* Draw rows of asterisks across the top and bottom of the screen */
  for (col = 0 ; col < 64 ; col++) {
    fbputchar('*', 0, col);
    fbputchar('*', 23, col);
  }

  /* Split the screen with a horizontal line between chat area and input box. */
  horizontal_line();

  /* Initialize keyboard via USB. */
  if ( (keyboard = openkeyboard(&endpoint_address)) == NULL ) {
    fprintf(stderr, "Did not find a keyboard\n");
    exit(1);
  }

  /* Allocate space for chat history pages. */
  alloc_new_text_page(&text_box_his);
  if(!text_box_his.pages[text_box_his.count-1]){
    perror(strerror(errno));
    exit(1);
  }
  printf("alloc %p\n", text_box_his.pages[text_box_his.count-1]);
  text_box_his.curr = 0;
  print_page_info(&text_box_his);

  /* Create a TCP socket */
  if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
    fprintf(stderr, "Error: Could not create socket\n");
    exit(1);
  }

  /* Prepare server address */
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(SERVER_PORT);
  if ( inet_pton(AF_INET, SERVER_HOST, &serv_addr.sin_addr) <= 0) {
    fprintf(stderr, "Error: Could not convert host IP \"%s\"\n", SERVER_HOST);
    exit(1);
  }

  /* Connect to the server */
  if ( connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    fprintf(stderr, "Error: connect() failed. Is the server running?\n");
    exit(1);
  }

  /* Start the receiving thread */
  pthread_create(&network_thread, NULL, network_thread_f, NULL);

  /* Main loop to read keyboard input */
  for (;;) {
    libusb_interrupt_transfer(
      keyboard, endpoint_address,
      (unsigned char *) &packet, sizeof(packet),
      &transferred, 0
    );
    if (transferred == sizeof(packet)) {
      int ret = handle_keyboard_input(&packet);
      if(ret){
        /* If ESC is pressed, clear screen and break. */
        if (ret == 0x29) { 
          clear_screen();
          break;
        }
      }
    }
  }

  /* End the receiver thread */
  pthread_cancel(network_thread);

  /* Wait for it to finish */
  pthread_join(network_thread, NULL);

  /* Free memory of pages */
  destroy_pages(&text_box_his);

  return 0;
}

/*=============================================*
 * Thread function for repeating a held key.   *
 * This is started by handle_key_hold().       *
 *=============================================*/
void *keyhold_thread_f(void *args)
{
  /* Optional initial delay before repeating starts */
  usleep(500 * 1000);  // 500 ms initial delay

  printf("Thread arg %02x %02x\n", hold_key, hold_modifiers);

  while(1){
    /* Repeatedly call handle_key_press() for the held key */
    handle_key_press(hold_key, hold_modifiers);
    usleep(KEY_HOLD_INTERVAL * 1000);  
  }
  return NULL;
}

/*===================================================*
 * Start repeating a newly pressed key in a thread.  *
 *===================================================*/
void handle_key_hold(char keycode, char modifiers)
{
  release_key_hold(); // Always stop any previous hold first.

  hold_key = keycode;
  hold_modifiers = modifiers;

  /* Launch a thread to repeatedly input or delete, etc. */
  pthread_create(&keyhold_thread, NULL, keyhold_thread_f, NULL);
  printf("Thread created! key=%02x, mod=%02x\n", keycode, modifiers);
}

/*======================================*
 * Stop repeating the currently held key *
 *======================================*/
void release_key_hold()
{
  if(keyhold_thread){
    pthread_cancel(keyhold_thread);
    pthread_join(keyhold_thread, NULL);
    keyhold_thread = 0;
  }
  hold_key = 0;
  // Optional debug: printf("Thread released!\n");
}

/*==============================================*
 * handle_keyboard_input(): compare old vs. new *
 *   - Find keys that were released             *
 *   - Find keys newly pressed                 *
 *==============================================*/
int handle_keyboard_input(struct usb_keyboard_packet *packet)
{
  /* A static variable to remember the previous keyboard state */
  static struct usb_keyboard_packet prev_state = {
    .modifiers = 0,
    .keycode = {0}
  };
  int r = 0;

  /*---------------------------------------*
   * 1) Identify which keys were released  *
   *---------------------------------------*/
  for(int i = 0; i < 6; i++){
    uint8_t old_key = prev_state.keycode[i];
    if(old_key == 0) continue;

    /* Search if old_key is still in packet->keycode[] now */
    int found = 0;
    for(int j = 0; j < 6; j++){
      if(packet->keycode[j] == old_key){
        found = 1;
        break;
      }
    }
    if(!found){
      // old_key is not in the new state => key release
      r = handle_key_release(old_key, packet->modifiers);
    }
  }

  /*---------------------------------------*
   * 2) Identify which keys are newly pressed
   *---------------------------------------*/
  for(int i = 0; i < 6; i++){
    uint8_t new_key = packet->keycode[i];
    if(new_key == 0) continue; // skip empty slot

    /* Check if new_key was already in prev_state */
    int found = 0;
    for(int j = 0; j < 6; j++){
      if(prev_state.keycode[j] == new_key){
        found = 1;
        break;
      }
    }
    if(!found){
      // This is a freshly pressed key
      handle_key_press(new_key, packet->modifiers);

      // Start repeating for the newly pressed key
      // (stop any previous hold, then hold the new key).
      release_key_hold();
      handle_key_hold(new_key, packet->modifiers);
    }
  }

  /*---------------------------------------*
   * 3) Update prev_state and return
   *---------------------------------------*/
  prev_state = *packet;
  return r;
}

/*=====================================*
 * handle_key_press(): logic for when  *
 * a key is pressed (once or repeated).*
 *=====================================*/
int handle_key_press(char keycode, char modifiers)
{
  struct position new_pos = msg_pos;

  switch (keycode) {
  case KEY_CAPSLOCK:
    caps_lock = 1 - caps_lock;  // toggle
    break;

  case KEY_ESC:
    // Return the keycode so that the main loop can detect ESC
    return keycode;

  case KEY_LEFT:
    /* If Ctrl + Left => switch page. Otherwise, move cursor left. */
    if(modifiers & KEY_MOD_CTRL)
      page_left();
    else
      cursor_left(&new_pos);
    break;

  case KEY_RIGHT:
    /* If Ctrl + Right => switch page. Otherwise, move cursor right. */
    if(modifiers & KEY_MOD_CTRL)
      page_right();
    else
      cursor_right(&new_pos);
    break;

  case KEY_UP:
    /* Move cursor up if possible. */
    if(new_pos.row > MSG_START_ROW+1){
      --new_pos.row;
      new_pos.buf_idx -= MAX_COLS;
    }
    break;

  case KEY_DOWN:
    /* Move cursor down if possible, but not beyond MSG_END_ROW - 1. */
    if(new_pos.row < MSG_END_ROW-1){
      int len = strlen(msgbuffer);
      if(new_pos.buf_idx + MAX_COLS > len){
        new_pos.row = MSG_START_ROW+1 + len/MAX_COLS;
        new_pos.col = len % MAX_COLS;
        new_pos.buf_idx = len;
      } else {
        ++new_pos.row;
        new_pos.buf_idx += MAX_COLS;
      }
    }
    break;

  case KEY_TAB:
    /* Insert several spaces on TAB */
    for (int i=0; i<TAB_SPACE; i++) {
      print_char(' ', &new_pos, msgbuffer);
    }
    break;

  case KEY_ENTER:
    /* Pressing ENTER => send the input message, then clear the input line. */
    async_send_message(msgbuffer);
    for (int i=MSG_START_ROW+1; i<MSG_END_ROW; i++) {
      put_line(' ', i);
    }
    new_pos.row = MSG_START_ROW+1;
    new_pos.col = 0;
    new_pos.buf_idx = 0;
    break;

  case KEY_BACKSPACE:
    /* If Ctrl + Backspace => clear entire line. Otherwise, delete one char. */
    if (CONTROL_PRESSED(modifiers)) {
      put_line(' ', MSG_START_ROW+1);
      put_line(' ', MSG_START_ROW+2);
      memset(msgbuffer, 0, BUFFER_SIZE+1);
      new_pos.col = 0;
      new_pos.row = MSG_START_ROW+1;
    } else {
      handle_back_space(keycode, modifiers, &new_pos);
    }
    break;

  default:
    /* For regular character keys: convert to ASCII and print. */
    print_char(keycode_to_char(keycode, modifiers), &new_pos, msgbuffer);
    break;
  }

  update_cursor(&new_pos);
  return 0;
}

/*=====================================*
 * handle_key_release(): logic for when
 * a key is released. Stop long-press
 * repeating if it is that key.
 *=====================================*/
int handle_key_release(char keycode, char modifiers)
{
  // If the released key is the one currently in "hold_key",
  // end the repetition thread.
  if(hold_key == keycode){
    release_key_hold();
  }
  return 0;
}

/*======================================================*
 * Additional logic for text display, cursor, pagination
 *======================================================*/

void reload_txt_box()
{
  char *start = text_box_his.pages[text_box_his.curr];
  int offset = 0;
  text_pos.row = 1;
  text_pos.col = 0;

  clear_txt_box();
  for(offset = 0; offset < TXT_BOX_LINES * MAX_COLS; offset += MAX_COLS){
    if(offset > 0) {
      if(start[offset-1] != 0)
        continue;
    }
    fbputs_wrap(start + offset, &text_pos, message_type(start + offset));
    if(text_pos.col){
      ++text_pos.row;
      text_pos.col = 0;
    }
  }
  print_page_info(&text_box_his);
}

void page_left()
{
  if(text_box_his.curr){
    --text_box_his.curr;
    reload_txt_box();
  }
}

void page_right()
{
  if(text_box_his.curr < text_box_his.count-1){
    ++text_box_his.curr;
    reload_txt_box();
  }
}

void debug_save_previous_page(char *page, int lineLen, int lines)
{
  FILE *fp = fopen("test.txt","a");
  if(!fp) return;
  char *buf = malloc(lineLen+1);
  fprintf(fp, "PAGE\n");
  for(int i=0; i<lines; ++i){
    memcpy(buf, page + lineLen*i, lineLen);
    buf[lineLen] = 0;
    if(buf[strlen(buf)-1] == '\n')
      fprintf(fp, "%s", buf);
    else
      fprintf(fp, "%s\n", buf);
  }
  fprintf(fp, "\n");
  fclose(fp);
  free(buf);
}

/*==========================================*
 * Send message asynchronously (in a thread)
 *==========================================*/
void async_send_message(char *msg)
{
  if(msg == NULL) return;
  if(!strlen(msg)) return;

  pthread_create(&network_thread_send, NULL, network_thread_s, (void *)msg);
}

/*============================================*
 * Thread that writes the message over the socket
 *============================================*/
void *network_thread_s(void *msg)
{
  char *p = (char *)msg;
  if(!write(sockfd, p, strlen(p)+1)){
    char *err = strerror(errno);
    perror(err);
    fbputs(err, 2, 0);
  } else {
    printf("Message sent: %s\n", p);
  }
  memset(msgbuffer, 0, BUFFER_SIZE+1);
  return NULL;
}

/*==========================================*
 * Thread to read inbound network messages.
 * Appends them to the text box area.
 *==========================================*/
void *network_thread_f(void *ignored)
{
  char recvBuf[BUFFER_SIZE];
  int n;
  int len;
  while ((n = read(sockfd, &recvBuf, BUFFER_SIZE - 1)) > 0 ) {
    recvBuf[n] = '\0';
    printf("%s\n", recvBuf);
    fflush(stdout);
    len = strlen(recvBuf);

    /* If the incoming message overflows current page, create a new page. */
    if(text_box_his.line + len / MAX_COLS >= TXT_BOX_LINES){
      debug_save_previous_page(text_box_his.pages[text_box_his.count-1], MAX_COLS, TXT_BOX_LINES);
      alloc_new_text_page(&text_box_his);
      if(text_box_his.curr < 0){
        text_box_his.curr = 0;
        reload_txt_box();
      }
      if(text_box_his.count - 2 == text_box_his.curr){
        text_pos.row = 1;
        text_pos.col = 0;
        ++text_box_his.curr;
        reload_txt_box();
      } else {
        print_page_info(&text_box_his);
      }
      text_box_his.line = 0;
    }
    memcpy(
      text_box_his.pages[text_box_his.count-1] + text_box_his.line * MAX_COLS,
      recvBuf,
      len
    );
    if(text_box_his.count-1 == text_box_his.curr){
      fbputs_wrap(recvBuf, &text_pos, message_type(recvBuf));
      if(text_pos.col){
        ++text_pos.row;
        text_pos.col = 0;
      }
      text_box_his.line = text_pos.row - 1;
    } else {
      text_box_his.line += len / MAX_COLS + 1;
    }
  }
  return NULL;
}

/*==========================================*
 * message_type() for color-coded printing
 * Here it always returns 0, meaning default
 *==========================================*/
int message_type(char *message)
{
  return 0; // for your usage, you can differentiate sender or such
}

/*==========================================*
 * handle_back_space(): remove 1 character 
 * (or entire line if ctrl+backspace).
 *==========================================*/
void handle_back_space(char keycode, char modifiers, struct position *new_pos)
{
  if(!new_pos->buf_idx) return;
  cursor_left(new_pos);       // Move the cursor left
  msgbuffer[new_pos->buf_idx] = 0;
  int idx = new_pos->buf_idx + 1;

  if(msgbuffer[idx] == 0){
    /* If we were deleting the last char, just clear that position on screen */
    fbputchar(' ', new_pos->row, new_pos->col);
  } else {
    /* If we are in the middle of the linift everything left by 1 char */
    while(msgbuffer[idx]){
      msgbuffer[idx - 1] = msgbuffer[idx];
      ++idx;
    }
    msgbuffer[idx-1] = 0;
    /* Now refresh the entire input line on the screen. */
    struct position tmp;
    tmp.col = 0;
    tmp.row = MSG_START_ROW + 1;
    for (int i = MSG_START_ROW+1; i < MSG_END_ROW; i++){
      put_line(' ', i);
    }
    fbputs_wrap(msgbuffer, &tmp, 0);
  }
}

/*==========================================*
 * update_cursor(): remove the old cursor, 
 * move to new_pos, then draw a new cursor.
 *==========================================*/
void update_cursor(struct position *new_pos)
{
  struct RGB888 black = {0,0,0},
                white = {255,255,255};

  draw_cursor(&msg_pos, black);   // Erase old cursor
  msg_pos = *new_pos;
  draw_cursor(&msg_pos, white);   // Draw new cursor
}

/*==========================================*
 * Move cursor left, adjusting row/col/buf_idx
 *==========================================*/
void cursor_left(struct position *pos)
{
  // If already at the first row + first column, do nothing
  if (pos->row == MSG_START_ROW+1 && pos->col == 0) {
    return;
  } 
  // If at col=0 but not on the first row, move up one row
  else if (pos->col == 0) {
    pos->row--;
    pos->col = MAX_COLS - 1;
    pos->buf_idx--;
    return;
  }
  pos->col--;
  pos->buf_idx--;
}

/*==========================================*
 * Move cursor right, adjusting row/col/buf_idx
 *==========================================*/
void cursor_right(struct position *pos)
{
  if(pos->buf_idx+1 >= BUFFER_SIZE) return;  // Reached input buffer limit
  if(msgbuffer[pos->buf_idx] == 0) return;   // No more text to the right

  // If at last column of the first input row, go to next row
  if (pos->row == MSG_START_ROW+1 && pos->col == MAX_COLS - 1){
    ++pos->row;
    pos->col = 0;
  } else {
    ++pos->col;
  }
  ++pos->buf_idx;
}

/*==========================================*
 * print_char(): place a char in msgbuffer 
 * and show it on screen at the correct row/col,
 * possibly dealing with wrapping or insertion.
 *==========================================*/
void print_char(char key, struct position *pos, char *msg_buf)
{
  if(key == 0) return;

  /* If cursor is at the very bottom-right, we do a small "shift up" logic */
  if (pos->row == MSG_END_ROW - 1 && pos->col == MAX_COLS - 1) {
    // Place the char
    msg_buf[pos->buf_idx] = key;
    // Shift the first line up in the buffer
    memcpy(msg_buf, msg_buf + MAX_COLS, MAX_COLS);
    memset(msg_buf + MAX_COLS, 0, MAX_COLS);

    // Refresh the line on screen
    for (int i=0; i<MAX_COLS; i++){
      fbputchar(msg_buf[i], pos->row - 1, i);
    }
    // Clear the bottom line
    put_line(' ', pos->row);

    // Reset cursor to the bottom line's last col
    pos->row = MSG_START_ROW + 1;
    pos->col = MAX_COLS - 1;
    pos->buf_idx = MAX_COLS - 1;
  } 
  else if (pos->buf_idx < strlen(msg_buf)) {
    /* If cursor is in the middle, we insert by shifting everything after it */
    int i = strlen(msg_buf);
    if(i > BUFFER_SIZE - 2) i = BUFFER_SIZE - 2;

    for(; i > pos->buf_idx; --i){
      msg_buf[i] = msg_buf[i - 1];
    }
    struct position p = *pos;
    fbputs_wrap(msg_buf + pos->buf_idx, &p, 0);

    msg_buf[pos->buf_idx] = key;
    fbputchar(key, pos->row, pos->col);
    cursor_right(pos);
  } 
  else {
    /* Otherwise, just append at the end. */
    msg_buf[pos->buf_idx] = key;
    fbputchar(key, pos->row, pos->col);
    cursor_right(pos);
  }
}

/*==========================================*
 * Allocate a new text page in msg_history
 *==========================================*/
char *alloc_new_text_page(struct msg_history *his)
{
  if(his->count < 256){
    char *mem = malloc(TXT_BOX_LINES * MAX_COLS * sizeof(unsigned char));
    memset(mem, 0, TXT_BOX_LINES * MAX_COLS);
    his->pages[his->count] = mem;
    ++his->count;
    return mem;
  }

  char *mem = his->pages[0];
  for(int i=0; i<255; ++i){
    his->pages[i] = his->pages[i+1];
  }
  his->pages[255] = mem;
  memset(mem, 0, TXT_BOX_LINES*MAX_COLS);
  --his->curr;
  return mem;
}

void destroy_pages(struct msg_history *his)
{
  for(int i=0; i<his->count; ++i){
    free(his->pages[i]);
  }
}

