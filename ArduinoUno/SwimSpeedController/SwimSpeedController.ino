static unsigned int SHORT_TIME = 350; /* Microsecs */
static unsigned int LONG_TIME = 1050; /* Microsecs */
static unsigned long DELAY_BETWEEN_MESSAGES = 43; /* Milliseconds */

static String inputString = "";         // a string to hold incoming data
static boolean stringComplete = false;  // whether the string is complete

static unsigned int speed_demand = 123;
static unsigned int speed_actual = 456;

static const long serialTX_interval = 1000;
static unsigned long serialTX_previousMillis = 0;
static char TX_buffer[] = {'D', '0', '0', '0', 'A', '0', '0', '0', '#', '0', '0', '\n', '\0'};

void setup()
{
  Serial.begin(9600);
  // reserve 200 bytes for the inputString:
  inputString.reserve(200);
  
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

void
write_checksum(char *chars, byte cs)
{
  char msn = cs / 16;
  char lsn = cs % 16;
  if (msn < 0x0A)
  {
    msn += '0';
  }
  else
  {
    msn -= 0x0A;
    msn += 'A';
  }
  if (lsn < 0x0A)
  {
    lsn += '0';
  }
  else
  {
    lsn -= 0x0A;
    lsn += 'A';
  }
  chars[ 0 ] = msn;
  chars[ 1 ] = lsn;
}



void loop()
{
  unsigned long currentMillis = millis();
  byte cs;
  
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
  * Serial Transmission Time
  */
  if(currentMillis - serialTX_previousMillis >= serialTX_interval) 
  {
    // save the last time
    serialTX_previousMillis = currentMillis;
    int_to_chars(speed_demand, TX_buffer + 1);
    int_to_chars(speed_actual, TX_buffer + 5);
    cs = calc_checksum(TX_buffer, 9);
    write_checksum(TX_buffer + 9, cs);
    Serial.print(TX_buffer);
  }
    
  // print the string when a newline arrives:
  if (stringComplete) 
  {
    Serial.println(inputString);
    // clear the string:
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
