#include <TimerOne.h>

static const unsigned int RADIO_CYCLE_TIME = 350; /* Microsecs */
static const unsigned int RADIO_SHORT_CYCLE_COUNT = 1; /* 1x the CYCLE TIME */
static const unsigned int RADIO_LONG_CYCLE_COUNT = 3; /* 3x the CYCLE TIME */
static const unsigned long DELAY_BETWEEN_MESSAGES = 100; /* Milliseconds */
static const unsigned int FILTER_SIZE = 500;
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

static const long radioTX_interval = DELAY_BETWEEN_MESSAGES;
static unsigned long radioTX_previousMillis = 0;


static String inputString = "";         // a string to hold incoming data
static boolean stringComplete = false;  // whether the string is complete

static unsigned int speed_demand = 0;
static unsigned int speed_actual = 0;

static const long serialTX_interval = 1000;
static unsigned long serialTX_previousMillis = 0;
static char TX_buffer[] = {'D', '0', '0', '0', 'A', '0', '0', '0', '#', '0', '0', '0', '\n', '\0'};
static char RX_buffer[] = {'\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '0'};

static const long sample_interval = 10;
static unsigned long sample_previousMillis = 0;
static const int sensorPin = A0;
static byte speed_measured_filter[ FILTER_SIZE ];
static int smf_wr_idx = 0;

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

void setup()
{
  pinMode(10, OUTPUT);
  pinMode(11, OUTPUT);
  
  // set a timer of length 350 microseconds
  Timer1.initialize(RADIO_CYCLE_TIME);
  Timer1.attachInterrupt( radioTimerISR );
  
  Serial.begin(9600);
  // reserve 200 bytes for the inputString:
  inputString.reserve(200);
  
  for(int i = 0; i < FILTER_SIZE; i++)
  {
    speed_measured_filter[ i ] = 0;
  }
  
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

void
int_to_chars(int i, char *chars)
{
  chars[ 0 ] = '0' + i / 100;
  i %= 100;
  chars[ 1 ] = '0' + i / 10;
  i %= 10;  
  chars[ 2 ] = '0' + i;
}

byte
calc_checksum(char *chars, int len)
{
  byte cs = 0;
  while(len > 0)
  {
    cs ^= *chars;
    chars++;
    len--;
  }
  return cs;
}

int sample_count = 0;
int radio_message_count = 0;

void loop()
{
  unsigned long currentMillis = millis();
  byte cs;
  
  /*if (next_radio_bit == NULL)
  {
    radio_message_count++;
    next_radio_bit = slower_bits;
    radio_bit_countdown = 28;
  }
  
  if ((radio_message_count % 50) == 0)
  {
    Serial.print(radio_message_count);
    Serial.print(" : ");
    Serial.println(currentMillis);
  }*/
  
  
  /*
  * Time for Samping Sensor
  */
  if(currentMillis - sample_previousMillis >= sample_interval) 
  {
    // save the last time
    sample_previousMillis = currentMillis;
  
    float volts = 5.0 * analogRead(sensorPin) / 1023.0;
    volts = max(volts, 1.25);
    volts = min(volts, 3.05);
    float level = (volts - 1.25) * 30.0;
    speed_measured_filter[ smf_wr_idx ] = round(level);
    smf_wr_idx++;
    smf_wr_idx %= FILTER_SIZE;
    long avg_speed = 0;
    for(int idx = 0; idx < FILTER_SIZE; idx++)
    {
      avg_speed += speed_measured_filter[ idx ];
    }
    avg_speed /= FILTER_SIZE;
    
    speed_actual = avg_speed;    
    
    
    
    
    sample_count++;
    
    /*if (!(sample_count % 100))
    {
      Serial.println("100 samples");
    }*/
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
      if (((speed_demand == 0) && (speed_actual > 0)) ||
          ((speed_demand > 0) && (speed_actual == 0)))
      {
        next_radio_bit = onoff_bits;
        radio_bit_countdown = 28;        
      }
      else if (speed_demand > speed_actual)
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
  
  
  /*
  * Time for Serial Transmission
  */
  if(currentMillis - serialTX_previousMillis >= serialTX_interval) 
  {
    // save the last time
    serialTX_previousMillis = currentMillis;
    int_to_chars(speed_demand, TX_buffer + 1);
    int_to_chars(speed_actual, TX_buffer + 5);
    cs = calc_checksum(TX_buffer, 9);
    int_to_chars(cs, TX_buffer + 9);
    Serial.print(TX_buffer);
  }
    
  // print the string when a newline arrives:
  if (stringComplete) 
  {
    if (inputString.length() == 9)
    {
      inputString.toCharArray(RX_buffer, 9);
      cs = calc_checksum(RX_buffer, 5);
      byte cs2 = atoi(RX_buffer + 5);
      if (cs != cs2)
      {
        Serial.print("ERROR : Checksum should be ");
        Serial.println(cs);
      }
      else
      {
        speed_demand = atoi(RX_buffer + 1);
      }
    } 
    // Always clear the string 
    inputString = "";
    stringComplete = false; 
  }
}

/*
  SerialEvent occurs whenever a new data comes in the
 hardware serial RX.  This routine is run between each
 time loop() runs, so using delay inside loop can delay
 response.  Multiple bytes of data may be available.
 */
void serialEvent() 
{
  while (Serial.available()) 
  {
    // get the new byte:
    char inChar = (char)Serial.read();
    // add it to the inputString:
    inputString += inChar;
    // if the incoming character is a newline, set a flag
    // so the main loop can do something about it:
    if ((inChar == '\n') || (inChar == '\r'))
    {
      stringComplete = true;
    }
  }
}
