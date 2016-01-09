#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <modbus/modbus.h>

int
main(int argc, char* argv[])
{
   const int SLAVE_ID = 10;
   modbus_t *ctx = NULL;
   int rc;
   struct timeval tv;
   int i, j;

   ctx = modbus_new_rtu("/dev/ttyUSB0", 115200, 'N', 8, 1);

   //modbus_set_debug(ctx, TRUE);
   modbus_set_error_recovery(ctx,
                             MODBUS_ERROR_RECOVERY_LINK |
                             MODBUS_ERROR_RECOVERY_PROTOCOL);

   modbus_set_slave(ctx, SLAVE_ID);

   modbus_get_response_timeout(ctx, &tv);
   printf("Timeout = %d:%d\n", tv.tv_sec, tv.tv_usec);

   if (modbus_connect(ctx) == -1)
   {
      fprintf(stderr, "Connection failed: %s\n", modbus_strerror(errno));
      modbus_free(ctx);
      return -1;
   }

   printf("Connected. Sleeping...");
   sleep(5);
   printf("awake!\n");

   modbus_flush(ctx);

   uint16_t registers[ 10 ];

   uint16_t speed_demand = 0;

   /** HOLDING REGISTERS **/
   //for(i = 0; i < 10; i++)
   while(1)
   {
      rc = modbus_write_register(ctx,
                                 2,
                                 speed_demand++);
      usleep(125000);

      rc = modbus_read_registers(ctx,
                                 0,
                                 10,
                                 registers);

      if (rc == 10)
      {
         for(j = 0; j < 10; j++)
         {
            printf("[%04x]", registers[ j ]);
         }
         printf("\n");
      }
      usleep(125000);
   }

   modbus_close(ctx);
   modbus_free(ctx);

}
