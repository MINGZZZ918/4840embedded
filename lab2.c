/*
 *
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
 
 // ==================== NEW ====================
 // �~V��~^�~T��| ~A�~H�ASCII�~Z~D转�~M�表�~R~L�~G��~U�
 #define SHIFT_PRESSED(modifier) ((modifier) & 0x02)  // �~H��~V�Shift�~T��~X��~P��~L~I�~K
 
 char keycode_to_char(uint8_t keycode, uint8_t modifier, int caps);
 char ascii_kb_table[];
 char ascii_kb_table_caps[];
 // ==================== END NEW ====================
 //used to find the keyborad USB
 
 
 /*
  * References:
  *
  * https://web.archive.org/web/20130307100215/http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html
 *
  * http://www.thegeekstuff.com/2011/12/c-socket-programming/
  * 
  */
 
 int sockfd; /* Socket file descriptor */
 
 struct libusb_device_handle *keyboard;
 uint8_t endpoint_address;
 
 pthread_t network_thread;
 void *network_thread_f(void *);
 
 
 int main()
 {
   int err, col, count = 0, row = 20, end = 0, ROW_ONE = 20, ROW_TWO = 21;
   struct sockaddr_in serv_addr;
 
   struct usb_keyboard_packet packet;
   int transferred;
   char keystate[12], prev_state[12] ={0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
 
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
 //   fbputchar('*', 0, col);
 
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
 
   /* Connect the socket to the server */
   if ( connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
     fprintf(stderr, "Error: connect() failed.  Is the server running?\n");
     exit(1);
   }
 
   /* Start the network thread */
   pthread_create(&network_thread, NULL, network_thread_f, NULL);
 
   /* Look for and handle keypresses */
   for (;;) {
 
     if (count == 0) fbputchar('|', 20, 0);
    libusb_interrupt_transfer(keyboard, endpoint_address,
                               (unsigned char *) &packet, sizeof(packet),
                               &transferred, 0);
 
     if (transferred == sizeof(packet)) {
 
       for(int i=0; i<6; i++) keystate[i] = keycode_to_char(packet.keycode[i], packet.modifiers, 0);
 
       for(int i=0; i<6; i++){
         if (keystate[i] == 0x00 && prev_state[i] != 0x00) break; // button was released
 
         if(keystate[i] == prev_state[i]) continue; //button state did not change
 
         if (keystate[i] == 0x28){ // enter
           for(int i =0; i< 64; i++){
             fbputchar(' ', 20, i);
             fbputchar(' ', 21, i);
           }
           send(sockfd, message, end, 0);
           memset(message, 0, sizeof(message));
           count = 0;
           end = 0;
           break;
         }
 
         if (keystate[i] == 0x29) // need extra to jump out of other for loop
           break;
 
        if (keystate[i] == 0x50){ // left arrow
 
          if (count > 0){
            count --;
 
            if (count == 63){
              fbputchar(message[count], row, 0);
              fbputchar('|', --row, count);
            }
            else{
              fbputchar(message[count], row, count+1);
              fbputchar('|', row, count);
            }
          }
 printf("%d\n", count);
 printf("%d\n", end);         continue;
        }
 
        if (keystate[i] == 0x4f){ //right arrow
          if (count < 128 && message[count]){
            if(count == 63){
              fbputchar(message[count], row, count);
              fbputchar('|', ++row, count+1);
            }
            else{
              fbputchar(message[count], row, count);
              fbputchar('|', row, count+1);
            }
            count++;
          }
          continue;;
        }
 
        if (keystate[i] == 0x2a){
 
           if (count == 0) continue;
           for(int i=count; i<end; i++){
             if(i == end) break;
               message[i-1] = message[i];
               fbputchar(message[i], row, i);
             }
             fbputchar('|', row, count-1);
             fbputchar(' ', row, end);
             message[end-1] = 0;
             count --;
             end--;
             continue;
        }
 
 
        if(count > 127) continue; // max characters
 
         if (count == 64){
           if(keystate[i] != ' '){
             for(int j=0; count > 0; j++, count--){
               if(message[count] == ' '){
                 for(int i=0; count < 63; i++, count++){
                   fbputchar(message[count+1], row, i);
                   fbputchar(' ', row-1, count+1);
                   message[64+i] = message[count+1];
                   message[count+1] = ' ';
                 }
                 count = count+j;
                 if (count >= end) end = count;
                 break;
               }
             }
             end_row++;
           }
           if(count == 0){
             while(count < 63){
               fbputchar(message[count], row, count);
               message[count++] = 0;
             }
             if(count >= end) end = count;
           }
         }
         if(count >= 64){ //write letters 
 fbputchar(keystate[i], row, count);
           if(count < 127)
             fbputchar('|', row, count+1);
         }
 
         else {
           fbputchar(keystate[i], row, count);
           if(count < 63)
             fbputchar('|', row, count+1);
           else {
             row++;
             fbputchar('|', row, 0);
          }
         }
         if(count < end){
          for(int j = end; j > count; j--){
            message[j+1] = message[j];
          }
          fbputchar('|', row, count+1);
          fbputchar(message[i], row, count);
        }
        message[count++] = keystate[i];
        if (count >= end) end = count;
       }
       for(int i=0; i<6; i++) prev_state[i] = keystate[i];
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
 char keycode_to_char(uint8_t keycode, uint8_t modifier, int caps) {
   if(keycode > 0x67) return 0;
   if(keycode == 0x28) return 0x28;
   if(keycode == 0x29) return 0x29;
   if(keycode == 0x4f) return 0x4f;
   if(keycode == 0x50) return 0x50;
   if(keycode == 0x2a) return 0x2a;
   return (SHIFT_PRESSED(modifier) || caps) ?  //if shift, we go to use capital ABCDEFG...
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
 // ==================== END NEW ====================
 
 
 void shiftRows(char *recvBuf, char **rowBuf, int n){
     free(rowBuf[n]);
     for (int i=n; i>1;  i--){
 
         for(int j=0; j<64; j++){
          fbputchar(' ', i, j);
         }
 
         rowBuf[i] = rowBuf[i-1];
         fbputs(rowBuf[i], i, 0);
     }
 
     for(int j=0; j<64; j++){
          fbputchar(' ', 1, j);
     }
     rowBuf[1] = strdup(recvBuf);
     fbputs(rowBuf[1], 1, 0);
 }
 
 void *network_thread_f(void *ignored)
 {
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
 
 //    memset(recvBuf, 0, BUFFER_SIZE);
   }
   for (int i=1; i<8; i++){
    free(rowBuf[i]);
   }
   return NULL;
 }
