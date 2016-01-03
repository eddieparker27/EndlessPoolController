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
#include "time.h"

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
char ss_status = '0';
char ss_command = '0';
char control_status = '0';
char control_command = '0';

#define WORKOUT_RUN_STATE_INITIAL      (0)
#define WORKOUT_RUN_STATE_PAUSED       (1)
#define WORKOUT_RUN_STATE_RUNNING      (2)
#define WORKOUT_RUN_STATE_FINISHED     (3)

#define WORKOUT_SEQ_WARMUP             (0)
#define WORKOUT_SEQ_LOW_INTERVAL       (1)
#define WORKOUT_SEQ_HIGH_INTERVAL      (2)
#define WORKOUT_SEQ_WARMDOWN           (3)


typedef struct
{
   int running_state;
   unsigned long running_timer;
   unsigned long next_transition;
   int sequence;
   int interval_counter;
   int warmup_speed;
   int warmup_duration;
   int low_speed;
   int low_duration;
   int high_speed;
   int high_duration;
   int repeats;
   int warmdown_speed;
   int warmdown_duration;
} T_WORKOUT;

T_WORKOUT workout;


#define HTTP_REQ_BUF_LEN    (4096)
char HTTP_req_buf[ HTTP_REQ_BUF_LEN ];
#define HTTP_RSP_BUF_LEN    (4096)
char HTTP_rsp_buf[ HTTP_RSP_BUF_LEN ];
#define HTTP_CONTENT_BUF_LEN     (65536)
char HTTP_content_buf[ HTTP_CONTENT_BUF_LEN ];
int content_length = 0;


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
                  ptr2 = strstr(ptr, "?SS");
                  if (ptr2 != NULL)
                  {
                     if (ss_status != '1')
                     {
                        ss_command = '1';
                     }
                     else
                     {
                        ss_command = '0';
                     }
                  }
                  ptr2 = strstr(ptr, "?control=");
                  if (ptr2 != NULL)
                  {
                     if (ptr2[ 9 ] == '1')
                     {
                        control_command = '1';
                     }
                     else
                     {
                        control_command = '0';
                     }
                  }
                  ptr2 = strstr(ptr, "?workout=");
                  if (ptr2 != NULL)
                  {
                     char *ptr3 = ptr2 + 9;
                     workout.warmup_speed = atoi(ptr3);
                     ptr3 = strchr(ptr3, ',');
                     if (ptr3 != NULL)
                     {
                        ptr3++;
                        workout.warmup_duration = atoi(ptr3);
                        ptr3 = strchr(ptr3, ',');
                     }
                     if (ptr3 != NULL)
                     {
                        ptr3++;
                        workout.low_speed = atoi(ptr3);
                        ptr3 = strchr(ptr3, ',');
                     }
                     if (ptr3 != NULL)
                     {
                        ptr3++;
                        workout.low_duration = atoi(ptr3);
                        ptr3 = strchr(ptr3, ',');
                     }
                     if (ptr3 != NULL)
                     {
                        ptr3++;
                        workout.high_speed = atoi(ptr3);
                        ptr3 = strchr(ptr3, ',');
                     }
                     if (ptr3 != NULL)
                     {
                        ptr3++;
                        workout.high_duration = atoi(ptr3);
                        ptr3 = strchr(ptr3, ',');
                     }
                     if (ptr3 != NULL)
                     {
                        ptr3++;
                        workout.repeats = atoi(ptr3);
                        ptr3 = strchr(ptr3, ',');
                     }
                     if (ptr3 != NULL)
                     {
                        ptr3++;
                        workout.warmdown_speed = atoi(ptr3);
                        ptr3 = strchr(ptr3, ',');
                     }
                     if (ptr3 != NULL)
                     {
                        ptr3++;
                        workout.warmdown_duration = atoi(ptr3);
                        ptr3 = strchr(ptr3, ',');
                     }
                  }
                  ptr2 = strstr(ptr, "?workoutstate=");
                  if (ptr2 != NULL)
                  {
                     int state = atoi(ptr2 + 14);
                     if (state == WORKOUT_RUN_STATE_INITIAL)
                     {
                        workout.running_state = WORKOUT_RUN_STATE_INITIAL;
                        workout.running_timer = 0;
                        workout.sequence = WORKOUT_SEQ_WARMUP;
                        workout.next_transition =
                           workout.warmup_duration * 1000;
                        workout.interval_counter = 0;
                     }
                     else if ((state == WORKOUT_RUN_STATE_PAUSED) ||
                              (state == WORKOUT_RUN_STATE_RUNNING))
                     {
                        workout.running_state = state;
                     }
                  }
                  ptr2 = HTTP_content_buf;
                  ptr2 += sprintf(ptr2, "<html><body>");
                  ptr2 += sprintf(ptr2, "<h1>Main Status</h1>");
                  ptr2 += sprintf(ptr2, "<table border=\"1\"><tr><td>Start/Stop</td><td>Control</td><td>Demand Speed</td><td>Actual Speed</td></tr>");
                  ptr2 += sprintf(ptr2, "<tr><td>%c</td><td>%c</td><td>%d</td><td>%d</td></tr>", ss_status, control_status, current_demand_speed, actual_speed);
                  ptr2 += sprintf(ptr2, "</table>");
                  ptr2 += sprintf(ptr2, "<h1>Workout Details</h1>");
                  ptr2 += sprintf(ptr2, "<table border=\"1\"><tr><td>Parameter</td><td>Value</td></tr>");
                  ptr2 += sprintf(ptr2, "<tr><td>Warmup Speed</td><td>%d</td></tr>", workout.warmup_speed);
                  ptr2 += sprintf(ptr2, "<tr><td>Warmup Duration</td><td>%d</td></tr>", workout.warmup_duration);
                  ptr2 += sprintf(ptr2, "<tr><td>Low Speed</td><td>%d</td></tr>", workout.low_speed);
                  ptr2 += sprintf(ptr2, "<tr><td>Low Duration</td><td>%d</td></tr>", workout.low_duration);
                  ptr2 += sprintf(ptr2, "<tr><td>High Speed</td><td>%d</td></tr>", workout.high_speed);
                  ptr2 += sprintf(ptr2, "<tr><td>High Duration</td><td>%d</td></tr>", workout.high_duration);
                  ptr2 += sprintf(ptr2, "<tr><td>Repeats</td><td>%d</td></tr>", workout.repeats);
                  ptr2 += sprintf(ptr2, "<tr><td>Warmdown Speed</td><td>%d</td></tr>", workout.warmdown_speed);
                  ptr2 += sprintf(ptr2, "<tr><td>Warmdown Duration</td><td>%d</td></tr>", workout.warmdown_duration);
                  ptr2 += sprintf(ptr2, "<tr><td>Running State</td><td>%d</td></tr>", workout.running_state);
                  ptr2 += sprintf(ptr2, "<tr><td>Running Timer</td><td>%d</td></tr>", workout.running_timer);
                  ptr2 += sprintf(ptr2, "<tr><td>Seqeunce</td><td>%d</td></tr>", workout.sequence);
                  ptr2 += sprintf(ptr2, "<tr><td>Interval Counter</td><td>%d</td></tr>", workout.interval_counter);
                  ptr2 += sprintf(ptr2, "<tr><td>Next Transition</td><td>%d</td></tr>", workout.next_transition);
                  ptr2 += sprintf(ptr2, "</table>");
                  ptr2 += sprintf(ptr2, "</body></html>");
                  content_length = ptr2 - HTTP_content_buf;
                  ptr2 = HTTP_rsp_buf;
                  ptr2 += sprintf(ptr2, "HTTP/1.0 200 OK\r\n");
                  ptr2 += sprintf(ptr2, "Content-Type: text/html\r\n");
                  ptr2 += sprintf(ptr2, "Cache-Control: no-cache\r\n");
                  ptr2 += sprintf(ptr2, "Content-Length: %d\r\n",
                                  content_length);
                  ptr2 += sprintf(ptr2, "Connecttion: Close\r\n");
                  ptr2 += sprintf(ptr2, "\r\n");
                  memcpy(ptr2, HTTP_content_buf, content_length);
                  ptr2 += content_length;
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

   ULONG last_time;
   ULONG time;
   ULONG elapsed;

   last_time = get_ms();

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
         if (res >= 15)
         {
            int cs1 = calc_checksum(str, 12);
            int cs2 = atoi(str + 12);
            if (cs1 == cs2)
            {
               ss_status = str[ 1 ];
               control_status = str[ 2 ];
               current_demand_speed = atoi(str + 4);
               actual_speed = atoi(str + 8);
               printf("COUNTER: %06d : STRTSTOP: %c CTRL: %c DEM:%03d : ACT:%03d\n", 
                      counter,
                      ss_status,
                      control_status,
                      current_demand_speed,
                      actual_speed);

               counter++;

               if ((new_demand_speed != current_demand_speed) ||
                   (ss_status != ss_command) ||
                   (control_status != control_command))
               {
                  sprintf(str, "C%c%cD%03d#",
                          ss_command,
                          control_command,
                          new_demand_speed);
                  cs1 = calc_checksum(str, 8);
                  sprintf(str + 8, "%03d\n", cs1);
                  res = write(USB, str, 12);
               }
            }
         }
         /*else
         {
            printf("%s\n", str);
         }*/
      }

      time = get_ms();
      elapsed = time - last_time;
      last_time = time;


      if (workout.running_state == WORKOUT_RUN_STATE_RUNNING)
      {
         if (workout.sequence == WORKOUT_SEQ_WARMUP)
         {
            new_demand_speed = workout.warmup_speed;
         }
         else if (workout.sequence == WORKOUT_SEQ_LOW_INTERVAL)
         {
            new_demand_speed = workout.low_speed;
         }
         else if (workout.sequence == WORKOUT_SEQ_HIGH_INTERVAL)
         {
            new_demand_speed = workout.high_speed;
         }
         else if (workout.sequence == WORKOUT_SEQ_WARMDOWN)
         {
            new_demand_speed = workout.warmdown_speed;
         }

         workout.running_timer += elapsed;
         if (workout.running_timer >= workout.next_transition)
         {
            if (workout.sequence == WORKOUT_SEQ_WARMUP)
            {
               if (workout.repeats > 0)
               {
                  workout.interval_counter = 0;
                  workout.sequence = WORKOUT_SEQ_LOW_INTERVAL;
                  workout.next_transition =
                     workout.running_timer + workout.low_duration * 1000;
               }
            }
            else if (workout.sequence == WORKOUT_SEQ_LOW_INTERVAL)
            {
               workout.sequence = WORKOUT_SEQ_HIGH_INTERVAL;
               workout.next_transition =
                  workout.running_timer + workout.high_duration * 1000;
            }
            else if (workout.sequence == WORKOUT_SEQ_HIGH_INTERVAL)
            {
               workout.interval_counter++;
               if (workout.interval_counter >= workout.repeats)
               {
                  workout.sequence = WORKOUT_SEQ_WARMDOWN;
                  workout.next_transition =
                     workout.running_timer + workout.warmdown_duration * 1000;
               }
               else
               {
                  workout.sequence = WORKOUT_SEQ_LOW_INTERVAL;
                  workout.next_transition =
                     workout.running_timer + workout.low_duration * 1000;
               }
            }
            else if (workout.sequence == WORKOUT_SEQ_WARMDOWN)
            {
               workout.running_state = WORKOUT_RUN_STATE_FINISHED;
            }
         }
      }
   }
}
