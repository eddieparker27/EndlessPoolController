#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <termios.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

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

#define HTTP_REQ_BUF_LEN    (4096)
char HTTP_req_buf[ HTTP_REQ_BUF_LEN ];
#define HTTP_RSP_BUF_LEN    (4096)
char HTTP_rsp_buf[ HTTP_RSP_BUF_LEN ];

void*
web_server_thread(void* params)
{
   int sockfd, newsockfd, portno;
   socklen_t clilen;
   struct sockaddr_in serv_addr, cli_addr;
   int n;
   portno = 80;

   sockfd = socket(AF_INET, SOCK_STREAM, 0);
   if (sockfd < 0)
   {
      error("ERROR opening socket");
   }

   bzero((char *) &serv_addr, sizeof(serv_addr));
   serv_addr.sin_family = AF_INET;
   serv_addr.sin_addr.s_addr = INADDR_ANY;
   serv_addr.sin_port = htons(portno);
   if (bind(sockfd,
            (struct sockaddr *) &serv_addr,
            sizeof(serv_addr)) < 0)
   {
      error("ERROR on binding");
   }
   listen(sockfd, 5);

   while(TRUE)
   {
      clilen = sizeof(cli_addr);
      newsockfd = accept(sockfd,
                        (struct sockaddr *) &cli_addr,
                        &clilen);
      if (newsockfd < 0)
      {
         error("ERROR on accept");
      }
      bzero(HTTP_req_buf, HTTP_REQ_BUF_LEN);
      n = read(newsockfd, HTTP_req_buf, (HTTP_REQ_BUF_LEN - 1));
      if (n < 0) 
      {
         error("ERROR reading from socket");
      }
      //printf("Here is the message: [%s]\n", HTTP_req_buf);
      if (strncmp(HTTP_req_buf, "GET", 3) == 0)
      {
         char *ptr = strchr(HTTP_req_buf + 3, '/');
         if (ptr != NULL)
         {
            char *ptr2 = strchr(ptr, ' ');
            if (ptr2 != NULL)
            {
               *ptr2 = 0;
               printf("URL = [%s]\n", ptr);
               if (strncmp(ptr, "/controller", 11) == 0)
               {
                  ptr2 = strstr(ptr, "?demand=");
                  if (ptr2 != NULL)
                  {
                     new_demand_speed = atoi(ptr2 + 8);
                     printf("New Demand Speed = %d\n",
                            new_demand_speed);
                  }
                  ptr2 = HTTP_rsp_buf;
                  ptr2 += sprintf(ptr2, "HTTP/1.0 200 OK\r\n");
                  ptr2 += sprintf(ptr2, "Content-Type: text/html\r\n");
                  ptr2 += sprintf(ptr2, "Cache-Control: no-cache\r\n");
                  ptr2 += sprintf(ptr2, "Content-Length: 8\r\n");
                  ptr2 += sprintf(ptr2, "Connecttion: Close\r\n");
                  ptr2 += sprintf(ptr2, "\r\n");
                  ptr2 += sprintf(ptr2, "D%03dA%03d",
                                  current_demand_speed,
                                  actual_speed);
               }
               else
               {
                  ptr2 = HTTP_rsp_buf;
                  ptr2 += sprintf(ptr2, "HTTP/1.0 404 Not found\r\n");
                  ptr2 += sprintf(ptr2, "Connection: Close\r\n");
                  ptr2 += sprintf(ptr2, "\r\n"); 
               }
               n = write(newsockfd, HTTP_rsp_buf, (ptr2 - HTTP_rsp_buf));
               if (n < 0)
               {
                  error("ERROR writing to socket");
               }
            }
         }
      }
      close(newsockfd);
   }
}

int
main(int argc, char *argv[])
{
   int res;
   int i;
   char str[ 512 ];
   int USB = -1;
   pthread_t tid;
   int r;

   r = pthread_create(&tid, NULL, &web_server_thread, NULL);
   if (r != 0)
   {
      printf("Can't create WebServer thread\n");
      return r;
   }


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

               if (new_demand_speed != current_demand_speed)
               {
                  sprintf(str, "D%03d#", new_demand_speed);
                  cs1 = calc_checksum(str, 5);
                  sprintf(str + 5, "%03d\n", cs1);
                  res = write(USB, str, 9);
               }
            }
         }
         /*else
         {
            printf("%s\n", str);
         }*/
      }
   }
}
