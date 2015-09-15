/* Testprogramm zum Ansteuern eines Alpine-Radios */

/* 

Vol Up    11010111 11011011 10101011 11011011 11010110 11010101
Vol Dn    11010111 11011011 10101011 01101101 11110110 11010101
Mute      11010111 11011011 10101011 10101101 11101110 11010101
Pst up    11010111 11011011 10101011 10101011 11101111 01010101
Pst dn    11010111 11011011 10101011 01010101 11111111 01010101
Source    11010111 11011011 10101011 10110111 11011011 01010101
Trk up    11010111 11011011 10101011 10111011 11011010 11010101
Trk dn    11010111 11011011 10101011 01011101 11111010 11010101
Power     11010111 11011011 10101011 01110111 11101011 01010101
Ent/Play  11010111 11011011 10101011 01010111 11111101 01010101
Band/prog 11010111 11011011 10101011 01101011 11110111 01010101

8ms HIGH + 4.5ms LOW = 12.5ms Prefix


*/

#define alpPin    10
#define sharanPin 12
#define debugLed  13

// common commands for alpine and sharan class
#define CMD_ERROR    0

#define CMD_VOL_UP   1
#define CMD_VOL_DOWN 2
#define CMD_TRK_UP   3
#define CMD_TRK_DOWN 4


class Alpine {
private:

  void sendToAlpine(boolean cmd[]) {
    //first send 8ms high
    digitalWrite(alpPin, HIGH);
    delayMicroseconds(8000);
    // send 4.5ms low
    digitalWrite(alpPin, LOW);
    delayMicroseconds(4500);
  
    for (int i = 0; i <= 47; i++) {
      //send bit for 0.5ms
      digitalWrite(alpPin, cmd[i] ? HIGH : LOW);
      delayMicroseconds(500);
      digitalWrite(alpPin, LOW);
      delayMicroseconds(500);
    }
    // send 41ms low
    digitalWrite(alpPin, LOW);
    delay(41); 
  }

  boolean volUp[48] =      {1,1,0,1,0,1,1,1, 1,1,0,1,1,0,1,1, 1,0,1,0,1,0,1,1, 1,1,0,1,1,0,1,1, 1,1,0,1,0,1,1,0, 1,1,0,1,0,1,0,1};
  boolean volDown[48] =    {1,1,0,1,0,1,1,1, 1,1,0,1,1,0,1,1, 1,0,1,0,1,0,1,1, 0,1,1,0,1,1,0,1, 1,1,1,1,0,1,1,0, 1,1,0,1,0,1,0,1};
  boolean trkUp[48] =      {1,1,0,1,0,1,1,1, 1,1,0,1,1,0,1,1, 1,0,1,0,1,0,1,1, 1,0,1,1,1,0,1,1, 1,1,0,1,1,0,1,0, 1,1,0,1,0,1,0,1};
  boolean trkDown[48] =    {1,1,0,1,0,1,1,1, 1,1,0,1,1,0,1,1, 1,0,1,0,1,0,1,1, 0,1,0,1,1,1,0,1, 1,1,1,1,1,0,1,0, 1,1,0,1,0,1,0,1};

public:
  void sendCmd(int cmd) {
    switch(cmd) {
      case CMD_VOL_UP:
        sendToAlpine(volUp);
        break;
      case CMD_VOL_DOWN:
        sendToAlpine(volDown);
        break;
      case CMD_TRK_UP:
        sendToAlpine(trkUp);
        break;
      case CMD_TRK_DOWN:
        sendToAlpine(trkDown);
        break;
    }
  }

};


class Sharan {
public:
  // 32bit values to be compared with the received command. will be initialized in constructor.
  unsigned long volUpCmd, volDownCmd, trkUpCmd, trkDownCmd;
  
  Sharan() {
    // bitwise commands
    boolean volUp[32] =    {0,1,0,0,0,0,0,1, 1,1,1,0,1,0,0,0, 1,0,0,0,0,0,0,0, 0,1,1,1,1,1,1,1, }; 
    boolean volDown[32] =  {0,1,0,0,0,0,0,1, 1,1,1,0,1,0,0,0, 0,0,0,0,0,0,0,0, 1,1,1,1,1,1,1,1, }; 
    boolean trkUp[32] =    {0,1,0,0,0,0,0,1, 1,1,1,0,1,0,0,0, 1,1,0,1,0,0,0,0, 0,0,1,0,1,1,1,1, }; 
    boolean trkDown[32] =  {0,1,0,0,0,0,0,1, 1,1,1,0,1,0,0,0, 0,1,0,1,0,0,0,0, 1,0,1,0,1,1,1,1, }; 

    // 'compress' them to a 32bit unsigned long for easy comparison
    for (int i=0; i<32; i++) {
      bitWrite(volUpCmd,i,volUp[i]);
      bitWrite(volDownCmd,i,volDown[i]);
      bitWrite(trkUpCmd,i,trkUp[i]);
      bitWrite(trkDownCmd,i,trkDown[i]);
    }
  }

private:

  int lastCmd=0;

  /*
   * Wait for the start mark. Wait forever if no start mark is received
   */
  void waitForStartMark() {
    for (;;) {
      int inputLevel;
      do {
        inputLevel=digitalRead(sharanPin);
      } while(inputLevel==HIGH);
  
      // wait 8ms with LOW signal until proper exit of function
      unsigned long lowStart;
      lowStart = micros();
      do {
        inputLevel=digitalRead(sharanPin);
      } while(inputLevel==LOW && ((micros()-lowStart)<8000) );

      // if we left the loop with input level LOW, a command starts here
      if (inputLevel==LOW) {
        return;    
      }
      // otherwise we loop again, waiting for a LOW signal
    }
  }

  /*
   * Receive the next 32 bit and store them in the receivedCmd.
   * Return the already with known commands compared bits as 
   * a command constant.
   */
  int receiveCommandData() {
    unsigned long receivedCmd;
    for (int i=0; i<32; i++) {
      unsigned long duration;

      // get the next HIGH pulse 
      duration = pulseIn(sharanPin,HIGH);
  
      // if about 600us we call this '0'
      if (duration >= 400 && duration <= 800) {
        bitClear(receivedCmd,i);
      } else 
      // if about 1600us we call this '1'
      if (duration >= 1400 && duration <= 1800) {
        bitSet(receivedCmd,i);
      } else
      // everthing else is a transmission error
      return CMD_ERROR;
    }    
    // if we sucessfully finished the loop, we try to match the 
    // data with the known commands
    if (receivedCmd == volUpCmd) {
      return CMD_VOL_UP;
    } else if (receivedCmd == volDownCmd) {
      return CMD_VOL_DOWN;
    } else if (receivedCmd == trkUpCmd) {
      return CMD_TRK_UP;
    } else if (receivedCmd == trkDownCmd) {
      return CMD_TRK_DOWN;
    } else {
      return CMD_ERROR;
    }
  }
 

public:

  /*
   * wait for the start mark and either receive a valid command, the 
   * repeat code or return the error code CMD_ERROR.
   */
  int getSharanCommand(void) {
    unsigned long duration;

    for(;;) {
      // wait for the start on sharan input pin
      waitForStartMark();
  
      // get the next HIGH pulse 
      duration = pulseIn(sharanPin,HIGH);
  
      // if about 2200us the last command is repeated
      if (duration >= 2000 && duration <= 2400) {
        if (lastCmd != CMD_ERROR) {
          return lastCmd;      
        }
      } else 
      // if about 4400us a new command transmission starts
      if (duration >= 4200 && duration <= 4600) {
        // try to receive the command bits and get decoded command ID
        int cmd = receiveCommandData();
        lastCmd=cmd; // even on error, we set the lastCmd.
        if (cmd != CMD_ERROR) {
          // remember the ID if it is repeated
          return cmd;
        }
      } 
      // When we reached end of the loop, something unexpected 
      // was send. We wait for new start mark.
    }
  }

};

Alpine alpine;
Sharan sharan;

void setup() {
  pinMode(alpPin, OUTPUT);
  pinMode(debugLed, OUTPUT);
  pinMode(sharanPin, INPUT);

  // wink hello with the LED to signals programm start.
  digitalWrite(debugLed,HIGH);
  delay(100);
  digitalWrite(debugLed,LOW);
  delay(100);
  digitalWrite(debugLed,HIGH);
  delay(100);
  digitalWrite(debugLed,LOW);
}

int last=-1;

void loop() {
  int cmd=sharan.getSharanCommand();
  digitalWrite(debugLed,HIGH);
  alpine.sendCmd(cmd);
  digitalWrite(debugLed,LOW);
}
