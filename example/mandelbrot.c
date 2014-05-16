// mandelbrot.c
// (c) Copyright Declan Malone, 2014
//
// Permission granted to modify and distribute this program under the
// terms of the GNU Public License v2 or later
//
// This program comes with no warranty
//

// Simple program to demonstrate using ANSI-like escapes to plot a
// portion of the Mandelbrot set on an ODROID SHOW TFT display.
//
// Requires a modified version of the Adafruit GFX module for Arduino
// GIT Download @ https://github.com/declanmalone/ODROID-SHOW.git

// Code to open the serial port taken from port_open.c in same repo
// (original code has no copyright info; will seek to clarify this)

#include <stdio.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>

#define baudrate	B500000

const char *default_port = "/dev/ttyUSB0";

int open_serial(const char *device)
{
  int fd;
  struct termios options;

  fd = open(device, O_RDWR | O_NOCTTY | O_NDELAY);

  if (fd < 0) return -1;

  tcgetattr(fd, &options);

  cfsetispeed(&options, baudrate);
  cfsetospeed(&options, baudrate);

  options.c_cflag |= CS8;
  options.c_iflag |= IGNBRK;
  options.c_iflag &= ~( BRKINT | ICRNL | IMAXBEL | IXON);
  options.c_oflag &= ~( OPOST | ONLCR );
  options.c_lflag &= ~( ISIG | ICANON | IEXTEN | ECHO | ECHOE | ECHOK |
			ECHOCTL | ECHOKE);
  options.c_lflag |= NOFLSH;
  options.c_cflag &= ~CRTSCTS;

  tcsetattr(fd, TCSANOW, &options);

  return fd;
}

// End of code derived from port_open.c

// Other useful includes (option handling)

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

// Constants relating to screen
#define COLS 320
#define ROWS 240

// global options

// amount of microseconds to wait after each write. See plot_point
// for description. These values seem to work OK:
//   using original spiwrite: usleep(5000)
//   with spiwrite_with_abandon: usleep(1200)
static int usleep_time = 1200;

// Colour palette (the various supported text colours).  The last
// colour in the list is the background colour. If the point is
// (probably) in in the set, we colour it with the background colour,
// otherwise we pick one of the other colours based on how many
// iterations it takes to figure out that it's not.

// For the list of colours, see
// http://odroid.com/dokuwiki/doku.php?id=en:odroidshow

const char text_palette[] =
  {
    '1', // red
    '2', // green
    '3', // yellow
    '4', // blue
    '5', // magenta
    '6', // cyan
    '7', // white
    '0', // black
  };

typedef enum { COLOUR_NA, COLOUR_SCALED, COLOUR_MOD,
	       COLOUR_TEST,
	       COLOUR_SENTINEL } colouring_method;

// window constants (a proper renderer would let you change these)
float x_left   = -2.5;
float x_right  = 1.0;
float y_bottom = -1.0;
float y_top    = 1.0;

// backing store for iterative rendering
float real_val[ROWS][COLS];
float imag_val[ROWS][COLS];

// plot a single point (zero -> success)
int plot_point(int fd, char colour, int x, int y) {

  static char buf[256];		// fixed-sized buffer can't be helped
  int offset, wrote, buf_in_use;

  buf_in_use = snprintf(buf, 256, "\e[3%cm\e[%d;%dX", colour, x, y);
  if (buf_in_use >= 256) {
    fprintf(stderr, "BUG: internal buffer overflow\n");
    exit(2);
  }
  //  printf("Sending '%s'\n",buf);

  //  buf_in_use -= 1; // spr(n)intf puts on a trailing zero
		      
  offset = 0;
  do {
    wrote = write(fd, buf + offset, buf_in_use);
    // we need to sleep for some time to prevent a problem with the
    // default ardino sketch spending too much time on SPI and causing
    // serial data overruns.
    //
    // I've modified the sketch to use spiwrite_with_abandon in
    // certain places to reduce the time spent on doing SPI
    // processing. Then I've tested the delays below to see how little
    // delay I can get away with here.
    usleep(usleep_time);
    if (wrote == -1) {
      if (errno == EAGAIN) {
	fprintf(stderr, "EAGAIN... rewriting\n");
	continue;
      } else {
	perror("Failed to write to serial port");
	exit(3);
      }
    } else {
      buf_in_use -= wrote;
      offset += wrote;
      if (buf_in_use) 
	fprintf(stderr, "short write (sent %d, %d remain)\n", 
		wrote, buf_in_use);
    }
  } while (buf_in_use);

}

void mandelbrot(int fd, int maxiter, int painter, int iteratively)
{

  // calculate some variables for translating pixel coordinates into
  // points on the complex plane (pixel 0,0 mapping to the complex
  // point x_left, y_bottom

  float x_delta = (x_right - x_left)   / (float) COLS;
  float y_delta = (y_top   - y_bottom) / (float) ROWS;

  float x0,y0; // complex value
  float temp,x,y;

  // palette-related
  int  palette_size = sizeof(text_palette);
  char colour;
  // scale factor used for linear scaling (multiplied by iter later)
  float scale_factor = (float) (palette_size - 2) / (float) maxiter;

  int i,j, iter, cindex;

  if (iteratively) {

    for (i=0; i < COLS; ++i) {
      for (j=0; j < ROWS; ++j) {
	real_val[i][j] = 0.0;
	imag_val[i][j] = 0.0;
      }
    }

    iter = 0;

  } else {

    for (x0 = x_left, i=0; i < COLS; ++i, x0 += x_delta) {//COLS
      for (y0 = y_bottom, j=0; j < ROWS; ++j, y0 += y_delta) {//ROWS

	x = 0.0; y = 0.0; iter = 0;

	while ( x*x + y*y < 4.0 && iter < maxiter ) {
	    temp = x*x - y*y + x0;
	    y    = 2*x*y + y0;
	    x    = temp;
	    iter++;
	}
	switch (painter) {
	case COLOUR_MOD:
	  colour = text_palette
	    [ iter >= maxiter ? palette_size - 1 : iter % (palette_size-1) ];
	  break;
	case COLOUR_SCALED:	// linear scale; log would be much better
	  if (iter >= maxiter) {
	    cindex = palette_size - 1;
	  } else {
	    cindex = (int) floor(0.5 + (float) iter * scale_factor);
	    assert(cindex < palette_size - 1);
	  }
	  colour = text_palette[cindex];
	  break;
	case COLOUR_TEST:
	  colour = text_palette[(i+j) % (palette_size - 1)];
	  break;
	default:
	  fprintf(stderr, "BUG: Unknown colouring method.\n");
	  exit(2);
	}

	// plot the point
	plot_point(fd, colour, i, j);

      }
    }
  }
}

int main(int argc, char *argv[])
{
  extern int optind, opterr, optopt;

  int opt, temp;

  int iterative = 0;
  colouring_method painter = COLOUR_MOD;
  int max_iter = 100;
  const char *port = default_port;

  int fd;

  while ((opt = getopt(argc, argv, "m:p:c:iu:")) != -1) {
    switch (opt) {
    case 'i':
      iterative = 1;
      break;
    case 'c':
      temp = atoi(optarg);
      if (temp == COLOUR_NA || temp >= COLOUR_SENTINEL ) {
	fprintf(stderr, "The -c option requires a numeric value of 1 or 2:\n");
	fprintf(stderr, " -c 1 uses a scaled colour palette.\n");
	fprintf(stderr, " -c 2 uses a modulo colour palette.\n");
	fprintf(stderr, " -c 2 uses a test  palette (not a fractal!).\n");
	fprintf(stderr, "(ignored invalid -c value)\n");
      } else {
	painter = temp;
      }
      break;
    case 'u':
      usleep_time = atoi(optarg);
      break;
    case 'm':
      temp = atoi(optarg);
      if (temp > 0) {
	max_iter = temp;
      } else {
	fprintf(stderr, "Argument to -m (max iter) must be more than zero!\n");
      }
      break;
    case 'p':
      port = optarg;
      break;
    default:
      fprintf(stderr,
	      "Usage: %s [-i] [-p port] [-c [12]] [-m max_iter] -u microseconds\n",
	      argv[0]);
      exit(1);
    }
  }

  if (optind < argc) {
    fprintf(stderr, "Extra argument(s) after options ignored\n");
  }

  printf("Opening serial port %s\n", port);
  fd = open_serial(port);
  if (fd < 0) {
    perror("Failed to open port: ");
    exit(1);
  }
  
  // put screen in a sane state (get back to NOTSPECIAL and set
  // correct orientation)
  write (fd, " \e0r", 4);
  usleep(500000); 		// much larger delay than we really need

  // clear the screen
  write (fd, "\e[2J\0", 4);
  usleep(500000);		// ditto

  mandelbrot(fd, max_iter, painter, iterative);

}
