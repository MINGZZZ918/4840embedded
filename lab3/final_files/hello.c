/*
 * Userspace program that communicates with the vga_ball device driver
 * through ioctls
 *
 * Stephen A. Edwards
 * Columbia University
 */

#include <stdio.h>
#include "vga_ball.h"
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

/* File descriptor for the VGA ball device */
int vga_ball_fd;

/**
 * Read and print the current background color
 * Uses the VGA_BALL_READ_BACKGROUND ioctl command
 */
void print_background_color() {
  vga_ball_arg_t vla;
  
  /* Call ioctl to read the background color from the device */
  if (ioctl(vga_ball_fd, VGA_BALL_READ_BACKGROUND, &vla)) {
      perror("ioctl(VGA_BALL_READ_BACKGROUND) failed");
      return;
  }  
  /* check what color is now */
  printf("Background color: %02x %02x %02x\n",
         vla.background.red, vla.background.green, vla.background.blue);
}

/**
 * Set the background color
 * Uses the VGA_BALL_WRITE_BACKGROUND ioctl command
 *
 */
void set_background_color(const vga_ball_color_t *c)
{
  vga_ball_arg_t vla;
  
  /* Copy the color to our argument structure */
  vla.background = *c;
  
  /* Call ioctl to write the background color to the device */
  if (ioctl(vga_ball_fd, VGA_BALL_WRITE_BACKGROUND, &vla)) {
      perror("ioctl(VGA_BALL_SET_BACKGROUND) failed");
      return;
  }
}


void print_ball_position() { // record the ball position
  vga_ball_arg_t vla;
  
  /* Call ioctl to read the ball position from the device */
  if (ioctl(vga_ball_fd, VGA_BALL_READ_POSITION, &vla)) {
      perror("ioctl(VGA_BALL_READ_POSITION) failed");
      return;
  }
  
  /* Print the current ball position */
  printf("Ball position: x=%d, y=%d\n", vla.position.x, vla.position.y);
}

/**
 * Set the ball position
 * Uses the VGA_BALL_WRITE_POSITION ioctl command
 */
void set_ball_position(unsigned short x, unsigned short y)
{
  vga_ball_arg_t vla;
  
  /* Set the position in our argument structure */
  vla.position.x = x;
  vla.position.y = y;
  
  /* Call ioctl to write the ball position to the device */
  if (ioctl(vga_ball_fd, VGA_BALL_WRITE_POSITION, &vla)) {
      perror("ioctl(VGA_BALL_WRITE_POSITION) failed");
      return;
  }
  printf("Ball position: x=%d, y=%d\n", vla.position.x, vla.position.y);

}

/**
 * Main function - opens the device and demonstrates ball movement
 */
int main()
{
  vga_ball_arg_t vla;
  int i;
  static const char filename[] = "/dev/vga_ball";  /* Device file name */

  /* Array of predefined colors we'll cycle through */
  static const vga_ball_color_t colors[] = {
    { 0xff, 0x00, 0x00 }, /* Red */
    { 0x00, 0xff, 0x00 }, /* Green */
    { 0x00, 0x00, 0xff }, /* Blue */
    { 0xff, 0xff, 0x00 }, /* Yellow */
    { 0x00, 0xff, 0xff }, /* Cyan */
    { 0xff, 0x00, 0xff }, /* Magenta */
    { 0x80, 0x80, 0x80 }, /* Gray */
    { 0x00, 0x00, 0x00 }, /* Black */
    { 0xff, 0xff, 0xff }  /* White */
  };

  #define COLORS 9  /* Number of colors in the array */

  /* Ball movement variables */
  unsigned short ball_x = 400;  /* Initial x position */
  unsigned short ball_y = 300;  /* Initial y position */
  short vel_x = 2;              /* X velocity (pixels per frame) */
  short vel_y = 2;              /* Y velocity (pixels per frame) */
  
  /* Screen boundaries */
  const unsigned short X_MAX = 639;        /* Maximum x coordinate */
  const unsigned short Y_MAX = 479;         /* Maximum y coordinate */
  const unsigned short BALL_SIZE = 30;      /* Ball radius in pixels */

  printf("VGA ball Userspace program started\n");

  /* Open the device file */
  if ((vga_ball_fd = open(filename, O_RDWR)) == -1) {
    fprintf(stderr, "could not open %s\n", filename);
    return -1;
  }

  /* Print the initial state */
  printf("Initial state: \n");
  print_background_color();
  print_ball_position();
  
  /* Set a random background color */
  srand(time(NULL));  /* Initialize random number generator */
  int color_index = rand() % COLORS;
 // set_background_color(&colors[color_index]);
  print_background_color();
  
  /* Animation loop - ball will bounce at boundaries */
  printf("Starting animation, press Ctrl+C to exit...\n");

  //this is the main loop that will run the bounce ball
  while (1) {
    //  Update ball position based on current velocity 
    ball_x += vel_x;
    ball_y += vel_y;
    
    // Check for collision with horizontal boundaries 
    if (ball_x <= BALL_SIZE || ball_x >= X_MAX - BALL_SIZE) {
      vel_x = -vel_x;  // Reverse x direction velocity 
      
      // Change background color randomly on collision 
      color_index = rand() % COLORS;
      set_background_color(&colors[color_index]);
    }
    
    // Check for collision with vertical boundaries 
    if (ball_y <= BALL_SIZE || ball_y >= Y_MAX - BALL_SIZE) {
      vel_y = -vel_y;  // Reverse y direction velocity 
      
      // Change background color randomly on collision 
      color_index = rand() % COLORS;
      set_background_color(&colors[color_index]);
      //but I dont check the corner case,it should be added? i am not sure about this
    }
    
    // Update the ball position in hardware 
    set_ball_position(ball_x, ball_y);
    
    // Sleep to maintain approximately 60 frames per second 
    usleep(25000);  // 1/60 second in microseconds 
  }
  
  printf("VGA BALL Userspace program terminating\n");
  return 0;
}
