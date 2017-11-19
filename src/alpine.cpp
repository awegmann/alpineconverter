#include <Arduino.h>
#include <TimerOne.h>

/*
 * Protocol converter for VW Sharan one wire to Alpine radio */
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

9ms HIGH + 4.5ms LOW = 13.5ms Prefix

Link to protocol description

http://www.sbprojects.com/knowledge/ir/nec.php
http://techdocs.altium.com/display/FPGA/NEC+Infrared+Transmission+Protocol


*/

// #define ATTINY_SETUP

#ifdef ATTINY_SETUP
  #undef DEBUG
  #define alpPin    2
  #define sharanPin 3
  #define debugLed  0
#else
  //#define DEBUG
  #define alpPin    10
  #define sharanPin 12
  #define debugLed  13
  #define directVolUp   7
  #define directVolDown 6
  #define directTrkUp   5
  #define directTrkDown 4
#endif

// common commands for alpine and sharan class
#define CMD_ERROR    0

#define CMD_VOL_UP   1
#define CMD_VOL_DOWN 2
#define CMD_TRK_UP   3
#define CMD_TRK_DOWN 4
#define CMD_REPEAT   5

volatile unsigned long commandToWrite=0;
volatile unsigned long commandCurrentlyWriting=0;
int tickCountHigh;
int tickCountLow;
int bitInCmdToSend;

#define START_SIGNAL -2
/*
According to the protocol description in
http://techdocs.altium.com/display/FPGA/NEC+Infrared+Transmission+Protocol

<cite>
When a key is pressed on the remote controller, the message transmitted consists of the following, in order:

a 9ms leading pulse burst (16 times the pulse burst length used for a logical data bit)
a 4.5ms space
the 8-bit address for the receiving device
the 8-bit logical inverse of the address
the 8-bit command
the 8-bit logical inverse of the command
a final 562.5µs pulse burst to signify the end of message transmission.

----

Logical '0' – a 562.5µs pulse burst followed by a 562.5µs space, with a total transmit time of 1.125ms
Logical '1' – a 562.5µs pulse burst followed by a 1.6875ms space, with a total transmit time of 2.25ms

</cite>

so 562.5µs is our time base for the interrupt handler because:
9ms is 16 * 0.5625ms
4.5ms is 8 * 0.5625ms
a logical 0 is 0.5625ms pulse + 0.5625ms space
a logical 1 is 0.5625ms pulse + 3 * 0.5625ms space

-> every clock is based on this 0.5625ms tick.
*/

void writePacketISR(void) {

  if (commandToWrite==0 && commandCurrentlyWriting==0) {
    return;
  }


  if (commandCurrentlyWriting==0) {
    // we are starting a new transmisstion
    bitInCmdToSend = START_SIGNAL;
    commandCurrentlyWriting = commandToWrite;
    commandToWrite = 0;
  }

  digitalWrite(debugLed, HIGH);
  if (bitInCmdToSend == START_SIGNAL) {
      digitalWrite(alpPin,HIGH);
      tickCountHigh = 16;
      tickCountLow = 8;
      bitInCmdToSend++;
  } else {
    if (tickCountHigh>0) {
      // as long as we have to send HIGH, we count down and wait
      tickCountHigh--;
      // HIGH finished, go to LOW
      if (tickCountHigh==0) {
        digitalWrite(alpPin,LOW);
      }
    } else if (tickCountLow>0) {
      // as long as we have to send LOW, we count down and wait
      tickCountLow--;
      if (tickCountLow==0) {
        // LOW finished, calculate next bit...
        bitInCmdToSend++;
        if (bitInCmdToSend<=32) {
          // as long as we have not send all bits, we prepare for the next.

          // LOW finished, go to HIGH state
          digitalWrite(alpPin,HIGH);

          tickCountHigh=1;
          // after 32 bits send (bitInCmdToSend==32) we handle a final pulse
          // as final signal
          if (bitInCmdToSend == 32 || !(bitRead(commandCurrentlyWriting,bitInCmdToSend))) {
            tickCountLow=1;
          } else {
            tickCountLow=3;
          }
        } else {
          digitalWrite(debugLed, LOW);

          tickCountHigh=0;
          tickCountLow=0;
          commandCurrentlyWriting=0;
        }
      }
    }
  }
}

class Alpine {
private:
/*
    //first send 8ms high
    digitalWrite(alpPin, HIGH);
    delayMicroseconds(9000);
    // send 4.5ms low
    digitalWrite(alpPin, LOW);
    delayMicroseconds(4500);

    for (int i = 0; i <= 32; i++) {
      //send bit for 0.5ms
      if (bitRead(cmd,i)) {
        digitalWrite(alpPin, HIGH);
        delayMicroseconds(560);
        digitalWrite(alpPin, LOW);
        delayMicroseconds(1690);
      } else {
        digitalWrite(alpPin, HIGH);
        delayMicroseconds(560);
        digitalWrite(alpPin, LOW);
        delayMicroseconds(560);
      }
    }
    // send 41ms low
    digitalWrite(alpPin, LOW);
    //delay(41);
*/

  void sendToAlpine(unsigned long cmd) {
    lastCmd=cmd;
    noInterrupts();
    commandToWrite = cmd;
    interrupts();
  }

  void sendRepeatCmdToAlpine() {
    //first send 8ms high
    digitalWrite(alpPin, HIGH);
    delayMicroseconds(9000);
    // send 2.2ms low
    digitalWrite(alpPin, LOW);
    delayMicroseconds(2250);

    // send 600us high
    digitalWrite(alpPin, HIGH);
    delayMicroseconds(560);
    // set to LOW
    digitalWrite(alpPin, LOW);
  }

  const unsigned long volUp   = 0xEB147286;
  const unsigned long volDown = 0xEA157286;
  const unsigned long trkUp   = 0xED127286;
  const unsigned long trkDown = 0xEC137286;

  unsigned long lastCmd = 0;

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
      case CMD_REPEAT:
        sendToAlpine(lastCmd);
        break;
    }
  }

  void attachInterrupt(TimerOne& timer) {
    timer.attachInterrupt(writePacketISR);
  }
};

class Sharan {
public:
  // {0,1,0,0,0,0,0,1, 1,1,1,0,1,0,0,0, 1,0,0,0,0,0,0,0, 0,1,1,1,1,1,1,1, };
  const unsigned long volUpCmd = 0xFE011782;

  // {0,1,0,0,0,0,0,1, 1,1,1,0,1,0,0,0, 0,0,0,0,0,0,0,0, 1,1,1,1,1,1,1,1, };
  const unsigned long volDownCmd = 0xFF001782;

  // {0,1,0,0,0,0,0,1, 1,1,1,0,1,0,0,0, 1,1,0,1,0,0,0,0, 0,0,1,0,1,1,1,1, };
  const unsigned long trkUpCmd = 0xF40B1782;

  // {0,1,0,0,0,0,0,1, 1,1,1,0,1,0,0,0, 0,1,0,1,0,0,0,0, 1,0,1,0,1,1,1,1, };
  const unsigned long trkDownCmd = 0xF50A1782;

  Sharan() {
  }

private:

  int lastCmd=0;

  // flag to remember if last command was issued by one of the 'direct pins'
  // if this happens, the next getSharanCommand call will wail for 110ms and
  // resets the flag.
  int lastCmdWasDirect=0;

  /*
   * Wait for the start mark or direct input pins. Wait forever if no start
   * mark is received or no input pin is triggered.
   * returns
   * 0: if start mark on sharanPin is received
   * CMD_VOL_UP: if directVolUp was triggered
   * CMD_VOL_DOWN: if directVolDown was triggered
   * CMD_TRK_UP: if directTrkUp was triggered
   * CMD_TRK_DOWN: if directTrkDown was triggered
   */
  int waitForStartMarkOrDirectPin() {

    for (;;) {
      int inputLevel;
      do {
        inputLevel=digitalRead(sharanPin);
        if (digitalRead(directVolUp)==LOW) {
          return CMD_VOL_UP;
        }
        if (digitalRead(directVolDown)==LOW) {
          return CMD_VOL_DOWN;
        }
        if (digitalRead(directTrkUp)==LOW) {
          return CMD_TRK_UP;
        }
        if (digitalRead(directTrkDown)==LOW) {
          return CMD_TRK_DOWN;
        }
      } while(inputLevel==HIGH);

      // wait 8ms with LOW signal until proper exit of function
      unsigned long lowStart;
      lowStart = micros();
      do {
        inputLevel=digitalRead(sharanPin);
      } while(inputLevel==LOW && ((micros()-lowStart)<8000) );

      // if we left the loop with input level LOW, a command starts here
      if (inputLevel==LOW) {
        return 0;
      }
      // otherwise we loop again, waiting for a LOW signal
      #ifdef DEBUG
        Serial.println("still HIGH signal");
      #endif
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

  /**
   * wait for the start mark and either receive a valid command, the
   * repeat code or return the error code CMD_ERROR.
   */
  int getSharanCommand(void) {
    unsigned long duration;

    // if last command was issued by direct pin, we have to wait for 110ms
    // until we return a new command. So, we do this here. This is not perfect,
    // but best simple effort.
    if (lastCmdWasDirect) {
      lastCmdWasDirect=0;
      delay(110);
    }

    for(;;) {
      // wait for the start on sharan input pin
      int startCmd=waitForStartMarkOrDirectPin();

      // if startCmd >0 one of the direct pins was triggered
      if (startCmd) {
        lastCmdWasDirect=1;
        // we can directly return startCmd
        return startCmd;
      } else {
        //digitalWrite(debugLed,HIGH);

        // get the next HIGH pulse
        duration = pulseIn(sharanPin,HIGH);

        // if about 2200us the last command is repeated
        if (duration >= 2000 && duration <= 2400) {
          if (lastCmd != CMD_ERROR) {
            // we wait as long as like a full packet was received, to get
            // the right timing.
            delay(110 - ( 9 +2 ));
            return CMD_REPEAT;
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
  }

};

Alpine alpine;
Sharan sharan;

void setup() {
  #ifdef DEBUG
    Serial.begin(57600);
  #endif
  #ifdef ARDUINO_AVR_GEMMA
    OSCCAL=0x41;
  #endif

  // setup timer interrupt service routine
  // clock tick set for 0.562 ms which is quite equal to 0.5625ms
  Timer1.initialize(562);
  Timer1.attachInterrupt(writePacketISR);

  pinMode(alpPin, OUTPUT);
  pinMode(debugLed, OUTPUT);
  pinMode(sharanPin, INPUT);

  // configure the 'direct input pins' (for debug purpose)
  pinMode(directVolUp, INPUT_PULLUP);
  pinMode(directVolDown, INPUT_PULLUP);
  pinMode(directTrkUp, INPUT_PULLUP);
  pinMode(directTrkDown, INPUT_PULLUP);

  // wink hello with the LED to signals programm start.
  digitalWrite(debugLed,HIGH);
  delay(100);
  digitalWrite(debugLed,LOW);
  delay(100);
  digitalWrite(debugLed,HIGH);
  delay(100);
  digitalWrite(debugLed,LOW);
}

byte last=-1;

void loop() {
  #ifdef DEBUG
    Serial.println("Starting loop");
  #endif

  int cmd=sharan.getSharanCommand();
  #ifdef DEBUG
    Serial.print("got cmd ");
    Serial.println(cmd);
  #endif
  alpine.sendCmd(cmd);
  //digitalWrite(debugLed,LOW);
}
