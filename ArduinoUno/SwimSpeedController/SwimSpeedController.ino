#include <TimerOne.h>
#include "ModbusSlave.h"

static const unsigned int RADIO_CYCLE_TIME = 350; /* Microsecs */
static const unsigned int RADIO_SHORT_CYCLE_COUNT = 1; /* 1x the CYCLE TIME */
static const unsigned int RADIO_LONG_CYCLE_COUNT = 3; /* 3x the CYCLE TIME */
static const unsigned long MIN_DELAY_BETWEEN_MESSAGES = 100; /* Milliseconds */
static const char *slower =  "0111100010100001011010000101";
static const char *faster =  "0111100010011001011010010101";

struct RADIO_BIT
{
  byte high_count;
  byte low_count;
};

static const RADIO_BIT onBIT = {RADIO_LONG_CYCLE_COUNT, RADIO_SHORT_CYCLE_COUNT};
static const RADIO_BIT offBIT = {RADIO_SHORT_CYCLE_COUNT, RADIO_LONG_CYCLE_COUNT};

static RADIO_BIT slower_bits[ 28 ];  
static RADIO_BIT faster_bits[ 28 ];

static RADIO_BIT* next_radio_bit = NULL;
static int radio_bit_countdown = 0;
static int radio_pin_state = LOW;
static const int radio_pin = 10;

static long radioTX_interval = MIN_DELAY_BETWEEN_MESSAGES;
static unsigned long radioTX_previousMillis = 0;

static const long sample_interval = 10;
static unsigned long sample_previousMillis = 0;
static const int speed_sensor_pin = A0;
static const int supply_voltage_sensor_pin = A1;

static long radio_ISR_counter = 0;

void radioTimerISR()
{
  radio_ISR_counter--;
  if (radio_ISR_counter < 1)
  {
    if (next_radio_bit != NULL)
    {
      if (radio_pin_state == LOW)
      {
        radio_ISR_counter = next_radio_bit->high_count;
        radio_pin_state = HIGH;
      }
      else
      {
        radio_ISR_counter = next_radio_bit->low_count;
        radio_pin_state = LOW;
        radio_bit_countdown--;
        if (radio_bit_countdown > 0)
        {
           next_radio_bit++;
        }
        else
        {
          next_radio_bit = NULL;
        }
      }
    }     
  }  
  digitalWrite(radio_pin, radio_pin_state);
}

static const int MODBUS_SLAVE_ID = 10;
static const long MODBUS_BAUD_RATE = 115200; 
static ModbusSlave mbs;

void setup()
{
  pinMode(10, OUTPUT);
  pinMode(11, OUTPUT);
  
  // set a timer of length 350 microseconds
  Timer1.initialize(RADIO_CYCLE_TIME);
  Timer1.attachInterrupt( radioTimerISR );
  
  mbs.setSlaveId( MODBUS_SLAVE_ID );
  mbs.config(&Serial, MODBUS_BAUD_RATE, SERIAL_8N1);
     
  for(int i = 0; i < 28; i++)
  {
    if (slower[ i ] == '1')
    {
      slower_bits[ i ] = onBIT;
    }
    else
    {
      slower_bits[ i ] = offBIT;
    }
    if (faster[ i ] == '1')
    {
      faster_bits[ i ] = onBIT;
    }
    else
    {
      faster_bits[ i ] = offBIT;
    }
  }
}

static word command_register = 0;
static word response_register = 0;
static word speed_demand = 0;
static word speed_actual = 0;
static const byte START_STOP_BIT = 0;
static const byte CONTROL_BIT = 1;

static bool control_command = false;
static bool control_response = false;

static word control_deadband = 10;
static word control_timeconst = 10000;

static word A0_filter_coeff = 32000;
static float A0_filter_value = 0;

static word A1_filter_coeff = 32000;
static float A1_filter_value = 0;

static word supply_voltage = 0;


void
setBitState(word* reg, byte b, bool val)
{
  word mask = (1 << b);
  *reg &= ~mask;
  if (val)
  {
    *reg |= mask;
  }
}

bool
getBitState(word* reg, byte b)
{
  return ((*reg & (1 << b)) != 0);
}


void loop()
{
  unsigned long currentMillis = millis();
  byte cs;
  
  /*
  * Time for Samping Sensor
  */
  if(currentMillis - sample_previousMillis >= sample_interval) 
  {
    // save the last time
    sample_previousMillis = currentMillis;
    float analogue_sample;
  
    analogue_sample = analogRead(speed_sensor_pin);
    analogue_sample *= (0x8000 - A0_filter_coeff);
    A0_filter_value = (analogue_sample + (A0_filter_value * A0_filter_coeff)) / 0x8000;
    speed_actual = A0_filter_value;

    analogue_sample = analogRead(supply_voltage_sensor_pin);
    analogue_sample *= (0x8000 - A1_filter_coeff);
    A1_filter_value = (analogue_sample + (A1_filter_value * A1_filter_coeff)) / 0x8000;
    supply_voltage = A1_filter_value;
  }

  /*
  * Copy control command to response
  */
  control_response = control_command;

  /*
  * Handle the modbus comms
  */
  setBitState(&response_register, CONTROL_BIT, control_response);
  mbs.registers[ 1 ] = response_register;  
  mbs.registers[ 3 ] = speed_actual;
  mbs.registers[ 4 ] = control_deadband;
  mbs.registers[ 5 ] = control_timeconst;
  mbs.registers[ 6 ] = A0_filter_coeff;
  mbs.registers[ 7 ] = supply_voltage;
  mbs.registers[ 8 ] = A1_filter_coeff;
  mbs.registers[ 9 ] = radioTX_interval & 0xFFFF;
  mbs.task();
  command_register = mbs.registers[ 0 ];
  speed_demand = mbs.registers[ 2 ];
  control_deadband = mbs.registers[ 4 ];
  control_timeconst = mbs.registers[ 5 ];
  A0_filter_coeff = min(mbs.registers[ 6 ], 0x7FFF);    
  A1_filter_coeff = min(mbs.registers[ 8 ], 0x7FFF);    
  control_command = getBitState(&command_register, CONTROL_BIT);
    
  /*
  * Calculate the radioTX_interval based on control error
  */
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
    // Act as if we sent a message as the speed is correct
    radioTX_previousMillis = currentMillis;
  }
    
  /*
  * Time for Radio Transmission
  */
  if(currentMillis - radioTX_previousMillis >= radioTX_interval) 
  {
    // save the last time
    radioTX_previousMillis = currentMillis;
    /* Only try sending if not already */
    if (next_radio_bit == NULL)
    {
      if (control_command)
      {
        if (speed_demand > speed_actual)
        {
          next_radio_bit = faster_bits;
          radio_bit_countdown = 28;
        }
        else if (speed_demand < speed_actual)
        {
          next_radio_bit = slower_bits;
          radio_bit_countdown = 28;
        }
      }
    }
  }
}


