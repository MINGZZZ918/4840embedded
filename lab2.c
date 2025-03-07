/*
 * CSEE 4840 Lab 2 for 2019
 *
 * Name/UNI:Noah Hartzfeld (nah2178)
          Zhengtao Hu (zh2651)
          Mingzhi Li (ml5160)
*/

#include "fbputchar.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "usbkeyboard.h"
#include <pthread.h>

/* Update SERVER_HOST to be the IP address of
* the chat server you are connecting to
*/
/* arthur.cs.columbia.edu */
#define SERVER_HOST "128.59.19.114"
#define SERVER_PORT 42000

#define BUFFER_SIZE 150

#define MAX_COL 63
#define MAX_CHAR 127
#define ROW_ONE 20
#define ROW_TWO 21


#define SHIFT_PRESSED(modifier) ((modifier) & (0x02 | 0x20))
char keycode_to_char(uint8_t keycode, uint8_t modifier);
char ascii_kb_table[];
char ascii_kb_table_caps[];
//used to find the keyborad USB


/*
* References:
*
* https://web.archive.org/web/20130307100215/http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html
*
opyright 2004-2015 Internet Systems Consortium.
All rights reserved.

*
*/

int sockfd; /* Socket file descriptor */

struct libusb_device_handle *keyboard;
uint8_t endpoint_address;

pthread_t network_thread;
void *network_thread_f(void *);


int main()
{
  int err, col, count = 0, row = ROW_ONE, end = 0, leave = 0, same = 1, zero_count = 0;
  struct sockaddr_in serv_addr;

  struct usb_keyboard_packet packet;
  int transferred;
  char keystate[12];
  char copy[21], prev_state[21]; memset(prev_state, 0, sizeof(prev_state));

  char  message[128]; memset(message, 0, sizeof(message));

  if ((err = fbopen()) != 0) {
    fprintf(stderr, "Error: Could not open framebuffer: %d\n", err);
    exit(1);
  }

  for (col = 0; col<64; col++){
    for (int row = 1; row<23; row++){
      fbputchar(' ', row, col);
    }
  }

  /* Draw rows of asterisks across the top and bottom of the screen */
  for (col = 0 ; col < 64 ; col++) {
    fbputchar('*', 0, col);
    fbputchar('*', 18, col) ;
    fbputchar('*', 23, col);
  }

  /* Open the keyboard */
  if ( (keyboard = openkeyboard(&endpoint_address)) == NULL ) {
    fprintf(stderr, "Did not find a keyboard\n");
    exit(1);
  }

  /* Create a TCP communications socket */
  if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
    fprintf(stderr, "Error: Could not create socket\n");
    exit(1);
  }

  /* Get the server address */
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(SERVER_PORT);
  if ( inet_pton(AF_INET, SERVER_HOST, &serv_addr.sin_addr) <= 0) {
    fprintf(stderr, "Error: Could not convert host IP \"%s\"\n", SERVER_HOST);
    exit(1);
  }

/*
  copyright 2004-2015 Internet Systems Consortium.
  All rights reserved.
*/
  if ( connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    fprintf(stderr, "Error: connect() failed.  Is the server running?\n");
    exit(1);
  }
  /* Start the network thread */
  pthread_create(&network_thread, NULL, network_thread_f, NULL);

      /* Look for and handle keypresses */
  for (;;) {

    if (count == 0)
      fbputchar('|', ROW_ONE, 0);

    libusb_interrupt_transfer(keyboard, endpoint_address,
      (unsigned char *) &packet, sizeof(packet), &transferred, 0);

    if (transferred == sizeof(packet)) {


      sprintf(copy, "%02x%02x%02x%02x%02x%02x%02x", packet.modifiers, packet.keycode[0], packet.keycode[1],
					packet.keycode[2], packet.keycode[3], packet.keycode[4], packet.keycode[5]);



      for(int i=0; i<6; i++)  // storing the current inputs
        keystate[i] = keycode_to_char(packet.keycode[i], packet.modifiers);
//for(int i = 0; i< 21; i++) printf("%c, %d\n", copy[i], i);

      for(int i=0, k=2; i<6; i++,k+=2){ // cycle through each key that was pressed, we may want to consider not caring about each press not totally sure
	same = 1;
	zero_count = 0;
	

	if(keystate[i] == 0xfc){
       // if (keystate[i] == 0x29){ //////// end session need extra to jump out of other for loop
          leave = 1;
          break;
        }

	for (int j = k; j<k+2; j++){
//	printf("aaaaaaaaa%c\n", copy[i]);
	  if (copy[j] != prev_state[j]){

	    same = 0;
            break;
	  }

	}

	if(same) continue;

	for(int j = k; j<k+2; j++){
//	printf("aaaaaaaaaaa%c, %d\n", copy[j], j);
	  if(copy[j] == '0' || copy[j]>0x67) zero_count++;
}

if (zero_count == 2) continue;
//printf("zerooooo %d\n", zero_count);	 if(zero_count == 2) continue;
	
/*
        if ((keystate[i] + 32 == prev_state[i])) // we did not release the key before pressing the shift
          break;

        if (keystate[i] == 0x00 && prev_state[i] != 0x00) // this key was released rather than pressed so we can ignore it
          break;

        if(keystate[i] == prev_state[i]) // key state did not change
          continue;
*/

        if (keystate[i] == 0xfb){ // enter was pressed

          for(int i=0; i<MAX_COL+1; i++){ // clear the display
            fbputchar(' ', ROW_ONE, i);
            fbputchar(' ', ROW_TWO, i);
          }

          if (count != end){

            for(int i=count; i<end; i++)
              message[i] = message[i+1];
          }

          message[end] = 0;

          send(sockfd, message, end, 0); // send the message
          memset(message, 0, sizeof(message)); // clear the stored message
          count = 0;
          end = 0;
          row = ROW_ONE;
          break; // end functionality and wait for more input
        }










        if (keystate[i] == 0xfe){ // left arrow

          if (count == 0) continue;
printf("messageeeeeeeeeeeeeeeeeeeeeeee %c\n", message[count-1]);
          message[count] = message[count-1];
printf("mesageeeeeeeeeeeeeeee %c\n", message[count]);
          message[count-1] = 0;

          if (count == MAX_COL+1){

            fbputchar('|', ROW_ONE, MAX_COL);
            fbputchar(message[count], ROW_TWO, 0);

            row = ROW_ONE;
          }

          else if (row == ROW_ONE){

            fbputchar('|', ROW_ONE, count-1);
            fbputchar(message[count], ROW_ONE, count);
        }

          else if (row == ROW_TWO){

            fbputchar('|', ROW_TWO, count-1);
            fbputchar(message[count], ROW_TWO, count);
          }

          count--;
          continue;
        }












        if (keystate[i] == 0xfd){ //right arrow

          if (count < MAX_CHAR && count < end){ // i think we can say just less than end but doesnt effect 

            message[count] = message[count+1];
            message[count+1] = 0;

            if (count == MAX_COL){

              fbputchar('|', ROW_TWO, 0);
              fbputchar(message[count], ROW_ONE, MAX_COL);
              row = ROW_TWO;
            }

            else if (row == ROW_ONE){

              fbputchar('|', ROW_ONE, count+1);
              fbputchar(message[count], ROW_ONE, count);
            }

            else if (row == ROW_TWO){
              fbputchar('|', ROW_TWO, count+1);
              fbputchar(message[count], ROW_TWO, count);
            }

            count++;
          }

          continue;
        }








        if (keystate[i] == 0xff){ // backspace need to figure out how to not collide with PPPPPPPPPP

          if (count > 0){

            if (count == MAX_COL + 1){ //cursor at row 2 require cursor shift to row 1

              fbputchar('|', ROW_ONE, MAX_COL);
              message[count-1] = message[count];

              for (int j = count+1; j <= end; j++) {

                message[j-1] = message[j];
                fbputchar(message[j-1], ROW_TWO, j-2);
              }

              row = ROW_ONE;
            }

            else if (row == ROW_TWO){  // cursor at row 2 not at very beginning

              // fbputchar('|', ROW_TWO, count-2); // cursor overwrite char before it
              for (int j = count; j <= end; j++) {

                message[j-1] = message[j];
                // fbputchar(message[j-2], ROW_TWO, j-2);
                fbputchar((message[j]) ? message[j] : '|', ROW_TWO, j-1);
              }
            }

            else if (row == ROW_ONE){  //cursor at row 1

              fbputchar('|', ROW_ONE, count-1);
              message[count-1] = message[count];

              for (int j = count+1; j <= end; j++){

                message[j-1] = message[j];

                if (j <= MAX_COL+1) // the first two cases can actually be identical
                  fbputchar(message[j-1], ROW_ONE, j-1);

                /*
                else if (j == MAX_COL + 1) // the first two cases can actually be identical
                  fbputchar(message[j-1], ROW_ONE, MAX_COL);
                */

                else
                  fbputchar(message[j-1], ROW_TWO, j-1);
              }
            }

            // last character gets doubled, but end is subtracted so its fine
            // could potentially make it zero

            if (end <= MAX_COL)
              fbputchar(' ', ROW_ONE, end);

            else
              fbputchar(' ', ROW_TWO, end);

            count--;
            end--;
          }

          continue;
        }










        // writing characters

        if (count >= MAX_CHAR) continue;

        if (end == count){


          message[count] = keystate[i];
          message[count+1] = 0; // this is how we keep track of the cursor in the message

          if (end == MAX_COL){

            fbputchar('|', ROW_TWO, 0);
            fbputchar(message[count], ROW_ONE, MAX_COL);

            row = ROW_TWO;
          }

          else if (row == ROW_ONE){

            fbputchar('|', ROW_ONE, count+1);
            fbputchar(message[count], ROW_ONE, count);
          }

          else if (row == ROW_TWO){

            //if (end < MAX_CHAR){ // dont need this already check for less than max_char at the beginning
            fbputchar('|', ROW_TWO, count+1); // dont need max_col any more but could still use it
            fbputchar(message[count], ROW_TWO, count); // dont need max_col any more but could still use it
          }

          // if (end < MAX_CHAR){ // same thing already checked this
          count++;
          end++;
        }

        else if (row == ROW_TWO){

          for (int i = end; i >= count ; i--){

            if (i+1 < MAX_CHAR){

              message[i+1] = message[i];
              fbputchar((message[i+1]) ? message[i+1] : '|', ROW_TWO, i); // dont need max_col any more but could still use it
            }
          }

          message[count] = keystate[i];
          fbputchar(message[count], ROW_TWO, count-1);

          count ++;
          if(end<MAX_CHAR) end++;
        }

        else if (row == ROW_ONE){

          for (int i = end; i> MAX_COL; i--){

            if (i+1 < MAX_CHAR){

              message[i+1] = message[i];
              fbputchar(message[i+1], ROW_TWO, i+1); // dont need max_col any more but could still use it
            }
          }

	  if(end >= MAX_COL){
            message[MAX_COL+1] = message[MAX_COL];
            fbputchar((message[MAX_COL+1]) ? message[MAX_COL+1] : '|', ROW_TWO, 0);
	  }
	  else{

	    message[end+1] = message[end];
	    fbputchar(message[end], ROW_ONE, end+1); 
	  }

            for (int i= (end > MAX_COL)?  MAX_COL-1 : end-1; i >= count; i--){

              message[i+1] = message[i];
              fbputchar((message[i+1]) ? message[i+1] : '|', ROW_ONE, i+1);
            }

            message[count] = keystate[i];
            fbputchar(message[count], ROW_ONE, count);

            count ++;
            if (count > MAX_COL) row = ROW_TWO;
            if(end<MAX_CHAR) end++;
  
        }
      }

      // printf("count %d\n", count);
      for(int i = 0; i<= end; i++) printf("%c, %d\n", message[i], i);

printf("count %d\n", count);
      if(leave) break;

      
      sprintf(prev_state, "%02x%02x%02x%02x%02x%02x%02x", packet.modifiers, packet.keycode[0], packet.keycode[1],
					packet.keycode[2], packet.keycode[3], packet.keycode[4], packet.keycode[5]);




//   for(int i=0; i<6; i++) prev_state[i] = keystate[i];
    }
  }

  /* Terminate the network thread */
  pthread_cancel(network_thread);

  /* Wait for the network thread to finish */
  pthread_join(network_thread, NULL);

  return 0;
}


// ==================== NEW ====================
// Added key code conversion functions and mapping tables
char keycode_to_char(uint8_t keycode, uint8_t modifier) {
  if(keycode > 0x67) return 0;
  if(keycode == 0x28) return 0xfb;
  if(keycode == 0x29) return 0xfc;
  if(keycode == 0x4f) return 0xfd; 
  if(keycode == 0x50) return 0xfe;
  if(keycode == 0x2a) return 0xff;

  return (SHIFT_PRESSED(modifier)) ?  //if shift, we go to use capital ABCDEFG...
        ascii_kb_table_caps[keycode] : ascii_kb_table[keycode];
}




char ascii_kb_table[] = {
 0  , 0  , 0  , 0  , 'a', 'b', 'c', 'd',
 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l',
 'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
 'u', 'v', 'w', 'x', 'y', 'z', '1', '2',
 '3', '4', '5', '6', '7', '8', '9', '0',
 '\n', 0 ,'\b','\t', ' ', '-', '=', '[',
 ']','\\', 0  , ';','\'', '`', ',', '.',
 '/', 0  , 0  , 0  , 0  , 0  , 0  , 0  ,
 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  ,
 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  ,
 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  ,
 0  , '1', '2', '3', '4', '5', '6', '7',
 '8', '9', '0', '.', 0  , 0  , 0  , '='
 };
 char ascii_kb_table_caps[] = {
 0  , 0  , 0  , 0  , 'A', 'B', 'C', 'D',
 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L',
 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',
 'U', 'V', 'W', 'X', 'Y', 'Z', '!', '@',
 '#', '$', '%', '^', '&', '*', '(', ')',
 '\n', 0 ,'\b','\t', ' ', '_', '+', '{',
 '}', '|', 0  , ':','\"', '~', '<', '>',
 '?', 0  , 0  , 0  , 0  , 0  , 0  , 0  ,
 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  ,
 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  ,
 0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  ,
 0  , '1', '2', '3', '4', '5', '6', '7',
 '8', '9', '0', '.', 0  , 0  , 0  , '='
 };
 // ==================== END NEW ==============










void shiftRows(char *recvBuf, char **rowBuf, int n){

  free(rowBuf[n]);
  for (int i=n; i>1;  i--){

      for(int j=0; j<64; j++)
        fbputchar(' ', i, j);

      rowBuf[i] = rowBuf[i-1];
      fbputs(rowBuf[i], i, 0);
  }
  for(int j=0; j<64; j++)
      fbputchar(' ', 1, j);

  rowBuf[1] = strdup(recvBuf);
  fbputs(rowBuf[1], 1, 0);
}

void *network_thread_f(void *ignored){
  char recvBuf[BUFFER_SIZE];
  int n;
  char *rowBuf[16];
  for (int i=1; i<16; i++){
    rowBuf[i] = strdup("");
  }
  /* Receive data */
  while ( (n = read(sockfd, &recvBuf, BUFFER_SIZE - 1)) > 0 ) {
    recvBuf[n] = '\0';
    printf("%s", recvBuf);

    if (n >= 129)
      shiftRows(&recvBuf[128], rowBuf, (sizeof(rowBuf)/sizeof(rowBuf[1]))-1);

    if (n>= 65){
      recvBuf[128] = '\0';
      shiftRows(&recvBuf[64], rowBuf, (sizeof(rowBuf)/sizeof(rowBuf[1]))-1);
    }

    recvBuf[64] = '\0';
    shiftRows(recvBuf, rowBuf, (sizeof(rowBuf)/sizeof(rowBuf[1]))-1);

    // memset(recvBuf, 0, BUFFER_SIZE);
  }

  for (int i=1; i<8; i++){
    free(rowBuf[i]);
  }

  return NULL;
}
