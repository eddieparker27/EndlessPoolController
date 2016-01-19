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

typedef int bool;
#define FALSE (1==0)
#define TRUE (1==1)

/* Table of CRC values for high–order byte */
static const unsigned char _auchCRCHi[] = 
{
   0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
   0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
   0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01,
   0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
   0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81,
   0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
   0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01,
   0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
   0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
   0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
   0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01,
   0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
   0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
   0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
   0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01,
   0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
   0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
   0x40
};

/* Table of CRC values for low–order byte */
static const unsigned char _auchCRCLo[] = 
{
   0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06, 0x07, 0xC7, 0x05, 0xC5, 0xC4,
   0x04, 0xCC, 0x0C, 0x0D, 0xCD, 0x0F, 0xCF, 0xCE, 0x0E, 0x0A, 0xCA, 0xCB, 0x0B, 0xC9, 0x09,
   0x08, 0xC8, 0xD8, 0x18, 0x19, 0xD9, 0x1B, 0xDB, 0xDA, 0x1A, 0x1E, 0xDE, 0xDF, 0x1F, 0xDD,
   0x1D, 0x1C, 0xDC, 0x14, 0xD4, 0xD5, 0x15, 0xD7, 0x17, 0x16, 0xD6, 0xD2, 0x12, 0x13, 0xD3,
   0x11, 0xD1, 0xD0, 0x10, 0xF0, 0x30, 0x31, 0xF1, 0x33, 0xF3, 0xF2, 0x32, 0x36, 0xF6, 0xF7,
   0x37, 0xF5, 0x35, 0x34, 0xF4, 0x3C, 0xFC, 0xFD, 0x3D, 0xFF, 0x3F, 0x3E, 0xFE, 0xFA, 0x3A,
   0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38, 0x28, 0xE8, 0xE9, 0x29, 0xEB, 0x2B, 0x2A, 0xEA, 0xEE,
   0x2E, 0x2F, 0xEF, 0x2D, 0xED, 0xEC, 0x2C, 0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26,
   0x22, 0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0, 0xA0, 0x60, 0x61, 0xA1, 0x63, 0xA3, 0xA2,
   0x62, 0x66, 0xA6, 0xA7, 0x67, 0xA5, 0x65, 0x64, 0xA4, 0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F,
   0x6E, 0xAE, 0xAA, 0x6A, 0x6B, 0xAB, 0x69, 0xA9, 0xA8, 0x68, 0x78, 0xB8, 0xB9, 0x79, 0xBB,
   0x7B, 0x7A, 0xBA, 0xBE, 0x7E, 0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C, 0xB4, 0x74, 0x75, 0xB5,
   0x77, 0xB7, 0xB6, 0x76, 0x72, 0xB2, 0xB3, 0x73, 0xB1, 0x71, 0x70, 0xB0, 0x50, 0x90, 0x91,
   0x51, 0x93, 0x53, 0x52, 0x92, 0x96, 0x56, 0x57, 0x97, 0x55, 0x95, 0x94, 0x54, 0x9C, 0x5C,
   0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E, 0x5A, 0x9A, 0x9B, 0x5B, 0x99, 0x59, 0x58, 0x98, 0x88,
   0x48, 0x49, 0x89, 0x4B, 0x8B, 0x8A, 0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D, 0x4C, 0x8C,
   0x44, 0x84, 0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42, 0x43, 0x83, 0x41, 0x81, 0x80,
   0x40
 };

#define BUF_LEN     (4096)
static unsigned char buf[ BUF_LEN ];

#define MBAP_HEADER_LEN     (7)

#define DEFAULT_SLAVE_ID    (10)
#define DEFAULT_PORT_NUM    (502)
#define DEFAULT_BAUD_RATE   (115200)
#define INTER_CHAR_DELAY    (2000) // micro seconds
#define RESPONSE_TIMEOUT    (1000000) // micro seconds

static int msg_len;

unsigned short
calc_crc(unsigned char address, unsigned char* pduFrame, unsigned char pduLen) 
{
   unsigned char CRCHi = 0xFF, CRCLo = 0x0FF, Index;
   
   Index = CRCHi ^ address;
   CRCHi = CRCLo ^ _auchCRCHi[Index];
   CRCLo = _auchCRCLo[Index];

   while (pduLen--) 
   {
      Index = CRCHi ^ *pduFrame++;
      CRCHi = CRCLo ^ _auchCRCHi[Index];
      CRCLo = _auchCRCLo[Index];
   }

   return (CRCHi << 8) | CRCLo;
}

   

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
   unsigned char mbap_unit_id;
   int enable = 1;
   unsigned char slave_id = DEFAULT_SLAVE_ID;
   unsigned short port_num = DEFAULT_PORT_NUM;
   int baud_rate = DEFAULT_BAUD_RATE;

   opterr = 0;
   while ((c = getopt (argc, argv, "s:p:b:")) != -1)
   {
      switch (c)
      {
      case 's':
        slave_id = atoi(optarg);
        break;
      case 'p':
        port_num = atoi(optarg);
        break;
      case 'b':
        baud_rate = atoi(optarg);
      default:
        abort ();
      }
   }
   printf ("modbus_gateway slave_id = %d, port_num = %d baud_rate = %d\n",
          slave_id, port_num, baud_rate);

   switch(baud_rate)
   {
   case 9600:
      baud_rate = B9600;
      break;
   case 19200:
      baud_rate = B19200;
      break;
   case 38400:
      baud_rate = B38400;
      break;
   case 57600:
      baud_rate = B57600;
      break;
   case 115200:
      baud_rate = B115200;
      break;
   case 230400:
      baud_rate = B230400;
      break;
   default:
      printf("Unsupported baud rate. Setting to 115200\n");
      baud_rate = B115200;
   }

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

   while(TRUE)
   {
      if (USB < 0)
      {         
         while(USB < 0)
         {
            USB = open("/dev/ttyUSB0", O_RDWR | O_NONBLOCK | O_NDELAY);

            if (USB < 0)
            {
               printf("Failed to open /dev/ttyUSB0 - error = %d (%s)\n", errno, strerror(errno));
            }
            sleep(5);
         }
         sleep(5);

         struct termios tty;
         memset(&tty, 0, sizeof(tty));

         if(tcgetattr(USB, &tty) != 0)
         {
            printf("Error %d from tcgetattr: %s\n", errno, strerror(errno));
         }

         cfsetospeed(&tty, baud_rate);
         cfsetispeed(&tty, baud_rate);
 
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
      }
      else
      {
         printf("Now accepting...");
         clilen = sizeof(cli_addr);
         newsockfd = accept(sockfd,
                           (struct sockaddr *) &cli_addr,
                           &clilen);
         if (newsockfd < 0)
         {
            error("ERROR on accept");
         }
     
         printf("connection accepted\n");
         do
         {
            printf("Waiting for incoming request\n");
            result = read_bytes(newsockfd, buf, (MBAP_HEADER_LEN - 1));
            if (result)
            {
               msg_len = (buf[ 4 ] << 8) | (buf[ 5 ]);
               result = read_bytes(newsockfd, buf, msg_len);
               if (result)
               {
				  bool testing_crc_problem = FALSE;
				  do
				  {
					  mbap_unit_id = buf[0];
					  buf[0] = slave_id;
					  crc = calc_crc(slave_id, buf + 1, msg_len - 1);
					  buf[msg_len++] = (crc >> 8);
					  buf[msg_len++] = (crc & 0xFF);

					  printf("SER REQ :: ");
					  for (i = 0; i < msg_len; i++)
					  {
						  printf("[%02x]", buf[i]);
					  }
					  printf("\n");

					  res = write(USB, buf, msg_len);
					  if (res != msg_len)
					  {
						  result = FALSE;
						  printf("Failed to write message to serial port!\n");
					  }
					  else
					  {
						  res = 0;
						  /*
						  * Wait for first char
						  */
						  int counter = RESPONSE_TIMEOUT / INTER_CHAR_DELAY;
						  i = 0;
						  while ((counter--) && (i == 0))
						  {
							  i = read(USB, buf + (MBAP_HEADER_LEN - 1), BUF_LEN);
							  usleep(INTER_CHAR_DELAY);
						  }
						  if (i < 0)
						  {
							  i = 0;
						  }
						  /*
						  * Get rest of message
						  */
						  while (i != 0)
						  {
							  res += i;
							  i = read(USB,
								  buf + res + (MBAP_HEADER_LEN - 1),
								  BUF_LEN - res - (MBAP_HEADER_LEN - 1));
							  if (i < 0)
							  {
								  i = 0;
								  res = 0;
							  }
							  usleep(INTER_CHAR_DELAY);
						  }
						  msg_len = 0;
						  printf("Read %d chars\n", res);
						  if (res > 0)
						  {
							  msg_len = res;

							  printf("SER RSP :: ");
							  for (i = MBAP_HEADER_LEN - 1; i < (msg_len + MBAP_HEADER_LEN - 1); i++)
							  {
								  printf("[%02x]", buf[i]);
							  }
							  printf("\n");

							  crc = calc_crc(buf[MBAP_HEADER_LEN - 1],
								  buf + MBAP_HEADER_LEN,
								  msg_len - 1);
							  if (crc != 0)
							  {
								  printf("CRC mismatch!!!\n");
								  testing_crc_problem = TRUE;
								  result = FALSE;
							  }
							  else
							  {
								  buf[MBAP_HEADER_LEN - 1] = mbap_unit_id;
								  msg_len -= 2; /* Knock off crc */
								  bzero(buf, (MBAP_HEADER_LEN - 1));
								  buf[1] = 1;
								  buf[4] = (msg_len >> 8);
								  buf[5] = msg_len & 0xFF;
								  msg_len += (MBAP_HEADER_LEN - 1); /* Add on MBAP */

								  printf("TCP RSP :: ");
								  for (i = 0; i < msg_len; i++)
								  {
									  printf("[%02x]", buf[i]);
								  }
								  printf("\n");

								  res = write(newsockfd, buf, msg_len);
								  if (res != msg_len)
								  {
									  result = FALSE;
								  }
								  else
								  {
									  result = TRUE;									  
								  }
							  }
						  }
						  else
						  {
							  result = FALSE;
						  }
					  }
				  } while (testing_crc_problem);
			   }
            }
         } while(result);
         
         printf("Resetting all connections\n");
         close(USB);
         USB = -1;
         close(newsockfd);
         newsockfd = -1;
      }
   }
}
