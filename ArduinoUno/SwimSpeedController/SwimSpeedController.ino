#include <TimerOne.h>
#include "ModbusSlave.h"

static const unsigned int RADIO_CYCLE_TIME = 350; /* Microsecs */
static const unsigned int RADIO_SHORT_CYCLE_COUNT = 1; /* 1x the CYCLE TIME */
static const unsigned int RADIO_LONG_CYCLE_COUNT = 3; /* 3x the CYCLE TIME */
static const unsigned long DELAY_BETWEEN_MESSAGES = 100; /* Milliseconds */
static const unsigned long SHORT_DELAY_BETWEEN_MESSAGE_BURSTS = 1000; /* Milliseconds */
static const unsigned long LONG_DELAY_BETWEEN_MESSAGE_BURSTS = 5000; /* Milliseconds */
static const unsigned long VERY_LONG_DELAY_BETWEEN_MESSAGE_BURSTS = 10000; /* Milliseconds */
static const unsigned long MESSAGE_BURST_COUNT = 4;
static const char *slower =  "0111100010100001011010000101";
static const char *faster =  "0111100010011001011010010101";
static const char *onoff =   "0111100010001001011010000101";

struct RADIO_BIT
{
  byte high_count;
  byte low_count;
};

static const RADIO_BIT onBIT = {RADIO_LONG_CYCLE_COUNT, RADIO_SHORT_CYCLE_COUNT};
static const RADIO_BIT offBIT = {RADIO_SHORT_CYCLE_COUNT, RADIO_LONG_CYCLE_COUNT};

static RADIO_BIT slower_bits[ 28 ];  
static RADIO_BIT faster_bits[ 28 ];
static RADIO_BIT onoff_bits[ 28 ];

static RADIO_BIT* next_radio_bit = NULL;
static int radio_bit_countdown = 0;
static int radio_pin_state = LOW;
static const int radio_pin = 10;

static long radioTX_interval = DELAY_BETWEEN_MESSAGES;
static unsigned long radioTX_previousMillis = 0;
static int radioTX_burst_counter = MESSAGE_BURST_COUNT;

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
    if (onoff[ i ] == '1')
    {
      onoff_bits[ i ] = onBIT;
    }
    else
    {
      onoff_bits[ i ] = offBIT;
    }
  }
}

int sample_count = 0;
int radio_message_count = 0;
static word command_register = 0;
static word response_register = 0;
static word speed_demand = 0;
static word speed_actual = 0;
static const byte START_STOP_BIT = 0;
static const byte CONTROL_BIT = 1;

static bool start_stop_command = false;
static bool start_stop_response = false;
static bool control_command = false;
static bool control_response = false;

static word control_deadband = 10;
static word control_slowband = 20;

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

                    
    sample_count++;    
  }

  /*
  * Copy control command to response
  */
  control_response = control_command;

  /*
  * Handle the modbus comms
  */
  setBitState(&response_register, START_STOP_BIT, start_stop_response);
  setBitState(&response_register, CONTROL_BIT, control_response);
  mbs.registers[ 1 ] = response_register;  
  mbs.registers[ 3 ] = speed_actual;
  mbs.registers[ 4 ] = control_deadband;
  mbs.registers[ 5 ] = control_slowband;
  mbs.registers[ 6 ] = A0_filter_coeff;
  mbs.registers[ 7 ] = supply_voltage;
  mbs.registers[ 8 ] = A1_filter_coeff;
  mbs.task();
  command_register = mbs.registers[ 0 ];
  speed_demand = mbs.registers[ 2 ];
  control_deadband = mbs.registers[ 4 ];
  control_slowband = mbs.registers[ 5 ];
  A0_filter_coeff = min(mbs.registers[ 6 ], 0x7FFF);    
  A1_filter_coeff = min(mbs.registers[ 8 ], 0x7FFF);    
  start_stop_command = getBitState(&command_register, START_STOP_BIT);
  control_command = getBitState(&command_register, CONTROL_BIT);
    
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
      if (start_stop_command != start_stop_response)
      {
        next_radio_bit = onoff_bits;
        radio_bit_countdown = 28;
        radioTX_interval = SHORT_DELAY_BETWEEN_MESSAGE_BURSTS;     
      }
      else if (control_command)
      {
        if (speed_demand > (speed_actual + control_deadband))
        {
          next_radio_bit = faster_bits;
          radio_bit_countdown = 28;
          if (speed_demand > (speed_actual + control_slowband))
          {
            radioTX_interval = SHORT_DELAY_BETWEEN_MESSAGE_BURSTS;        
          }
          else
          {
            radioTX_interval = LONG_DELAY_BETWEEN_MESSAGE_BURSTS;        
          }
        }
        else if (speed_demand < (speed_actual - control_deadband))
        {
          next_radio_bit = slower_bits;
          radio_bit_countdown = 28;
          if (speed_actual > (speed_demand + control_slowband))
          {
            radioTX_interval = SHORT_DELAY_BETWEEN_MESSAGE_BURSTS;        
          }
          else
          {
            radioTX_interval = LONG_DELAY_BETWEEN_MESSAGE_BURSTS;        
          }        
        }
      }
      
      radioTX_burst_counter--;
      if (radioTX_burst_counter > 0)
      {
        radioTX_interval = DELAY_BETWEEN_MESSAGES;
      }
      else
      {
        radioTX_burst_counter = MESSAGE_BURST_COUNT;
        start_stop_response = start_stop_command;
      }
    }
    else
    {
      radioTX_interval = DELAY_BETWEEN_MESSAGES;
    } 
  }
  
  
  /*
  * Time for Serial Transmission
  */
  /*if(currentMillis - serialTX_previousMillis >= serialTX_interval) 
  {
    // save the last time
    serialTX_previousMillis = currentMillis;
    TX_buffer[ 1 ] = ss_status;
    TX_buffer[ 2 ] = control_status;
    int_to_chars(speed_demand, TX_buffer + 4);
    int_to_chars(speed_actual, TX_buffer + 8);
    cs = calc_checksum(TX_buffer, 12);
    int_to_chars(cs, TX_buffer + 12);
    Serial.print(TX_buffer);
  }*/
    
  // print the string when a newline arrives:
  /*if (stringComplete) 
  {
    if (inputString.length() == 12)
    {
      inputString.toCharArray(RX_buffer, 12);
      cs = calc_checksum(RX_buffer, 8);
      byte cs2 = atoi(RX_buffer + 8);
      if (cs != cs2)
      {
        Serial.print("ERROR : Checksum should be ");
        Serial.println(cs);
      }
      else
      {
        ss_command = RX_buffer[ 1 ];
        control_command = RX_buffer[ 2 ];
        control_status = control_command;
        speed_demand = atoi(RX_buffer + 4);
      }
    } 
    // Always clear the string 
    inputString = "";
    stringComplete = false; 
  }*/
}


