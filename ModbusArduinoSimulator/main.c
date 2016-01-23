/* 
 * File:   main.c
 * Author: parkere
 *
 * Created on 21 January 2016, 11:24
 */

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
#include <semaphore.h>
#include <time.h>

typedef unsigned short word;
typedef unsigned long ULONG;
typedef int bool;
#define FALSE (1==0)
#define TRUE (1==1)
#define CONTROL_ACTIVE_BIT (0x0002)

ULONG
get_ms(void)
{
   ULONG ms;
   struct timespec tp;
   clock_gettime(CLOCK_REALTIME, &tp);

   ms = tp.tv_sec * 1000;
   ms += tp.tv_nsec / 1000000;

   return ms;
}



#define BUF_LEN     (1024)
static unsigned char tcp_buf[ BUF_LEN ];
static unsigned char ser_req_buf[ BUF_LEN ];
static unsigned char ser_rsp_buf[ BUF_LEN ];

#define max(a,b)     ((a) > (b) ? (a) : (b))

#define MIN_DELAY_BETWEEN_MESSAGES       (100)

#define MBAP_HEADER_LEN     (7)

#define DEFAULT_PORT_NUM    (502)   

bool
read_bytes(int sockfd, char* buf, int num)
{
   int result = TRUE;
   int n = 0;
   do
   {
      n = read(sockfd, buf, num);
      buf += n;
      num -= n;
      result = (n > 0);
   } while ((result) && (num > 0));
   return result;
}

int
main(int argc, char *argv[])
{
   int res;
   int i;
   int USB = -1;
   pthread_t tid;
   int r;
   int c;
   unsigned short crc;
   int sockfd, newsockfd;
   socklen_t clilen;
   struct sockaddr_in serv_addr, cli_addr;
   bool result;
   int serial_retries;
   int enable = 1;
   unsigned short port_num = DEFAULT_PORT_NUM;
   int msg_len;
   int ser_req_len;
   unsigned short MB_regs[ 100 ];
   word speed_demand = 0;
   word speed_actual = 0;
   word measured_speed_actual = 0;
   word filtered_measured_speed_actual = 0;
   word control_deadband = 0;
   word control_timeconst = 0;
   word radioTX_interval = 0;
   
   for(i = 0; i < 100; i++)
   {
       MB_regs[ i ] = 0;
   }
   
   /* Intializes random number generator */
   time_t t;
   srand((unsigned) time(&t));

   opterr = 0;
   while ((c = getopt (argc, argv, "p:")) != -1)
   {
      switch (c)
      {
        break;
      case 'p':
        port_num = atoi(optarg);
        break;
      default:
        abort ();
      }
   }
   printf ("port_num = %d\n",
          port_num);

   sockfd = socket(AF_INET, SOCK_STREAM, 0);
   if (sockfd < 0)
   {
      printf("ERROR opening socket - error = %d (%s)\n", errno, strerror(errno));
      return 1;
   }
   
   if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
   {
      printf("setsockopt(SO_REUSEADDR) failed - error = %d (%s)\n", errno, strerror(errno));
   }

   bzero((char *) &serv_addr, sizeof(serv_addr));
   serv_addr.sin_family = AF_INET;
   serv_addr.sin_addr.s_addr = INADDR_ANY;
   serv_addr.sin_port = htons(port_num);

   if (bind(sockfd,
            (struct sockaddr *) &serv_addr,
            sizeof(serv_addr)) < 0)
   {
      printf("ERROR on binding - error = %d (%s)\n", errno, strerror(errno));
      return(1);
   }
   listen(sockfd, 5);
   
   ULONG time;
   ULONG elapsed;
   ULONG last_time;

   if (last_time == 0)
   {
      last_time = get_ms();  
   }

   while(TRUE)
   {
      printf("Now accepting...");
      clilen = sizeof(cli_addr);
      newsockfd = accept(sockfd,
                        (struct sockaddr *) &cli_addr,
                         &clilen);
      if (newsockfd < 0)
      {
         printf("ERROR on accept: %s\n", errno, strerror(errno));
      }
     
      printf("connection accepted\n");
      do
      {
         printf("Waiting for incoming request\n");
         result = read_bytes(newsockfd, tcp_buf, (MBAP_HEADER_LEN - 1));
         if (result)
         {
            msg_len = (tcp_buf[ 4 ] << 8) | (tcp_buf[ 5 ]);
            result = read_bytes(newsockfd, 
                                tcp_buf + (MBAP_HEADER_LEN - 1), 
                                msg_len);
         }
         if (result)
         {
            memcpy(ser_req_buf, tcp_buf + MBAP_HEADER_LEN - 1, msg_len);
            ser_req_len = msg_len;
            printf("SER REQ :: ");
            for (i = 0; i < ser_req_len; i++)
            {
               printf("[%02x]", ser_req_buf[ i ]);
            }
            printf("\n");
            
            time = get_ms();
            elapsed = time - last_time;
            last_time = time;
            
            MB_regs[ 1 ] = MB_regs[ 0 ];
            speed_demand = MB_regs[ 2 ];
            control_deadband = MB_regs[ 4 ];
            control_timeconst = MB_regs[ 5 ];
            
            
            word control_error = abs(speed_actual - speed_demand);
            if (control_error > control_deadband)
            {
               radioTX_interval = control_timeconst / (control_error * control_error);
               radioTX_interval = max(radioTX_interval, MIN_DELAY_BETWEEN_MESSAGES);
            }
            else
            {
               // Set the interval to max
               radioTX_interval = 0xFFFF;
            }
            
            if (MB_regs[ 1 ] & CONTROL_ACTIVE_BIT)
            {
                if (speed_demand > speed_actual)
                {
                    int num = (6 * (rand() % 100) * elapsed); 
                    int denom = (radioTX_interval * 100);
                    printf("UP %d %d\n", num, denom);
                    speed_actual += num / denom;
                }
                else if (speed_demand < speed_actual)
                {
                    int num = (6 * (rand() % 100) * elapsed); 
                    int denom = (radioTX_interval * 100);
                    printf("DOWN %d %d\n", num, denom);
                    speed_actual -= num / denom;
                }                
            }
            
            MB_regs[ 3 ] = speed_actual;
            MB_regs[ 9 ] = radioTX_interval;
            
         
            /* Start with an echo response */
            memcpy(ser_rsp_buf, ser_req_buf, ser_req_len);
            msg_len = ser_req_len;
            
            if (ser_req_buf[ 1 ] == 0x03) // Read holding registers
            {                
                int start_addr = (ser_req_buf[ 2 ] << 8) | (ser_req_buf[ 3 ]);
                int num_addr = (ser_req_buf[ 4 ] << 8) | (ser_req_buf[ 5 ]);
                ser_rsp_buf[ 2 ] = num_addr * 2;
                int i;
                for(i = 0; i < num_addr; i++)
                {
                    ser_rsp_buf[ 3 + (i * 2) ] = MB_regs[ start_addr + i ] >> 8;
                    ser_rsp_buf[ 4 + (i * 2) ] = MB_regs[ start_addr + i ] & 0xFF;
                }
                msg_len = 3 + num_addr * 2;
            }
            else if (ser_req_buf[ 1 ] == 0x06) // Write holding register
            {
                int addr = (ser_req_buf[ 2 ] << 8) | (ser_req_buf[ 3 ]);
                MB_regs[ addr ] = (ser_req_buf[ 4 ] << 8) | (ser_req_buf[ 5 ]);
            }

            printf("SER RSP :: ");
            for (i = 0; i < msg_len; i++)
            {
               printf("[%02x]", ser_rsp_buf[i]);
            }
            printf("\n");

            memcpy(tcp_buf + MBAP_HEADER_LEN - 1, ser_rsp_buf, msg_len);
            tcp_buf[ 4 ] = (msg_len >> 8);
            tcp_buf[ 5 ] = msg_len & 0xFF;
            msg_len += (MBAP_HEADER_LEN - 1); /* Add on MBAP */
            printf("TCP RSP :: ");
            for (i = 0; i < msg_len; i++)
            {
               printf("[%02x]", tcp_buf[i]);
            }
            printf("\n");
            res = write(newsockfd, tcp_buf, msg_len);
            if (res != msg_len)
            {
               result = FALSE;
            }
         }
         else
         {
            result = FALSE;
         }               
      } while(result);
         
      printf("Resetting all connections\n");
      close(newsockfd);
      newsockfd = -1;
   }
}


