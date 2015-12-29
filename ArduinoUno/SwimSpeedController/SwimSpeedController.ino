static const unsigned int SHORT_TIME = 350; /* Microsecs */
static const unsigned int LONG_TIME = 1050; /* Microsecs */
static const unsigned long DELAY_BETWEEN_MESSAGES = 43; /* Milliseconds */
static const unsigned int FILTER_SIZE = 500;

static String inputString = "";         // a string to hold incoming data
static boolean stringComplete = false;  // whether the string is complete

static unsigned int speed_demand = 123;
static unsigned int speed_actual = 456;

static const long serialTX_interval = 1000;
static unsigned long serialTX_previousMillis = 0;
static char TX_buffer[] = {'D', '0', '0', '0', 'A', '0', '0', '0', '#', '0', '0', '0', '\n', '\0'};
static char RX_buffer[] = {'\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '0'};

static const long sample_interval = 10;
static unsigned long sample_previousMillis = 0;
static const int sensorPin = A0;
static byte speed_measured_filter[ FILTER_SIZE ];
static int smf_wr_idx = 0;


void setup()
{
  Serial.begin(9600);
  // reserve 200 bytes for the inputString:
  inputString.reserve(200);
  
  for(int i = 0; i < FILTER_SIZE; i++)
  {
    speed_measured_filter[ i ] = 0;
  }
  
  pinMode(10, OUTPUT);
  pinMode(11, OUTPUT);
}

void transmitt( int pin,
                char* code,
                unsigned int short_time,
                unsigned int long_time)
{
  char *c = code;
  unsigned int first_time = 0;
  unsigned int second_time = 0;
  while(*c != 0)
  {
    if (*c == '1')
    {
      first_time = long_time;
      second_time = short_time;            
    }
    else
    {
      first_time = short_time;
      second_time = long_time;
    }
    //noInterrupts();
    digitalWrite(pin, HIGH);
    delayMicroseconds(first_time);
    digitalWrite(pin, LOW);
    delayMicroseconds(second_time);
    //interrupts();
    c++;
  }
}

char *slower =  "0111100010100001011010000101";
char *faster =  "0111100010011001011010010101";
char *onoff =   "0111100010001001011010000101";

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
    
    if (!(sample_count % 100))
    {
      Serial.println("100 samples");
    }
  }
  
  
  /*delay(5000);
  Serial.print("Sending...");
  digitalWrite(11, HIGH);
  for(int i = 0; i < 40; i++)
  {
    delay(DELAY_BETWEEN_MESSAGES);
    transmitt(10, onoff, SHORT_TIME, LONG_TIME);
  }
  digitalWrite(11, LOW);
  Serial.println("...sent");*/
  
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
      // clear the string:
      inputString = "";
      stringComplete = false;
    }  
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
