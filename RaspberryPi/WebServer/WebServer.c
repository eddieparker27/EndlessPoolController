#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <termios.h>
#include <string.h>

#define FALSE (1==0)
#define TRUE (1==1)

unsigned char
calc_checksum(char *chars, int len)
{
  unsigned char cs = 0;
  while(len > 0)
  {
    cs ^= *chars;
    chars++;
    len--;
  }
  return cs;
}

int current_demand_speed = 0;
int actual_speed = 0;
int new_demand_speed = 0;

int
main(int argc, char *argv[])
{
   int res;
   int i;
   char str[ 512 ];


   int USB = -1;

   while(USB < 0)
   {
      USB = open("/dev/ttyUSB0", O_RDWR | O_NONBLOCK | O_NDELAY);

      if (USB < 0)
      {
         printf("Failed to open /dev/ttyUSB0 - error = %d (%s)\n", errno, strerror(errno));
      }
      sleep(5);
   }

   struct termios tty;
   memset(&tty, 0, sizeof(tty));

   if(tcgetattr(USB, &tty) != 0)
   {
      printf("Error %d from tcgetattr: %s\n", errno, strerror(errno));
   }

   cfsetospeed(&tty, B9600);
   cfsetispeed(&tty, B9600);
 
   tty.c_cflag     &=  ~PARENB;
   tty.c_cflag     &=  ~CSTOPB;
   tty.c_cflag     &=  ~CSIZE;
   tty.c_cflag     |=  CS8;
   tty.c_cflag     &=  ~CRTSCTS;
   tty.c_lflag     =  0;
   tty.c_oflag     =  0;
   tty.c_cc[VMIN]  =  0;
   tty.c_cc[VTIME] =  0;

   tty.c_cflag     |=  CREAD | CLOCAL;
   tty.c_iflag     &=  ~(IXON | IXOFF | IXANY);
   tty.c_lflag     &=  ~(ICANON | ECHO | ECHOE | ISIG);
   tty.c_oflag     &=  ~OPOST;

   tcflush(USB, TCIFLUSH);

   if (tcsetattr(USB, TCSANOW, &tty) != 0)
   {
      printf("Error %d from tcsetattr: %s\n", errno, strerror(errno));
   }

   tcflush(USB, TCIFLUSH);

   int counter = 0;

   while(TRUE)
   {
      usleep(250000);
      res = read(USB, str, 512);
      if (res < 0)
      {
         printf("Failed to read data\n");
      }
      else
      {
         if (res >= 12)
         { 
            int cs1 = calc_checksum(str, 9);
            int cs2 = atoi(str + 9);
            if (cs1 == cs2)
            {
               current_demand_speed = atoi(str + 1);
               actual_speed = atoi(str + 5);
               printf("%d : D%03d : A%03d\n", 
                      counter, current_demand_speed,
                      actual_speed);
               counter++;
            }
         }
         /*else
         {
            printf("%s\n", str);
         }*/
      }
   }
}
