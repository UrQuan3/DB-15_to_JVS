// Code for JVS, 1 or 2 player DB-15 interfaces, and 1 or 2 coin readers.
// All other interfaces are unmodified and untested. (UrQuan3)

// JVS protocol for adding controls to a Sega Naomi
// Intended for a Teensy 3.1 (probably overkill but I like them)
// Just need something with a hardware UART
// 		Pin 0 RX
// 		Pin 1 TX
// 		Pin 2 TXenable
// 		Pin 3 Sense output
//		Pins 14-23 controller inputs
// 	RX,TX,TXenable go to a RS485 module
//  I had a 120 ohm resistor between A+B but I now believe that's already on the RS485 module
// 	Sense output is the sense line to the JVS connector + top of 4 diodes to ground
//
// Schematic: 
//                  GND------------------------+
//                   |                         |  |-----------------|
// |-----------|     |    |--------------|     +--|GND            14|---Switch---GND
// |    GND   4|-----+----|GND         DI|--------|1              15|---Switch---GND
// |    D+    3|----------|A           DE|---+----|2              16|---Switch---GND
// |JVS        |          |    RS485     |   |    |     Teensy    17|---Switch---GND
// |    D-    2|----------|B          !RE|---+    |      3.1     ...|---Switch---GND
// |    Sense 1|--+     +-|VCC         R0|--------|0              21|---Switch---GND
// |-----------|  |     | |--------------|  +-----|Vin            22|---Switch---GND
//                |     |                   |  +--|3              23|---Switch---GND
//                |  5V-+-------------------+  |  |-----------------|
//                |                            |
//                +----------------------------+
//                | 
//                +-----Diode---Diode---Diode---Diode---GND

// Note, diode chain is only needed for device discovery when chaining together JVS nodes.
// As there is only a single node, I left them off. (UrQuan3)

#include <Bounce2.h>
#define BOUNCE_LOCK_OUT
Bounce coindebouncer1 = Bounce();
Bounce coindebouncer2 = Bounce();
//Bounce coindebouncer3 = Bounce(); //for if I need it
//Bounce coindebouncer4 = Bounce();

#define PIN_TXENABLE	2
#define PIN_SENSE	3
#define PIN_COIN_1 4
#define PIN_COIN_2 5
//#define PIN_COIN_3 ?  //for if I need it
//#define PIN_COIN_4 ?
//#define PIN_LED		13

//#define PLAYER_1
#define PLAYER_2
//#define PLAYER_4


//#define LOG(a) Serial.println(a)
#define LOG(a) (void)0

enum
{
  CMD_RESET = 0xF0,			// Always followed by 0xD9
  CMD_SETADDRESS = 0xF1,
  CMD_SETMETHOD = 0xF2,

  CMD_READID = 0x10,
  CMD_FORMATVERSION = 0x11,
  CMD_JVSVERSION = 0x12,
  CMD_COMMSVERSION = 0x13,
  CMD_GETFEATURES = 0x14,
  CMD_SETMAINBOARDID = 0x15,

  CMD_READSWITCHES = 0x20,
  CMD_READCOIN = 0x21,
  CMD_READANALOG = 0x22,
  CMD_READROTARY = 0x23,
  CMD_READKEYCODE = 0x24,
  CMD_READSCREENPOS = 0x25,
  CMD_READGPIO = 0x26,

  CMD_WRITEPAYOUTREMAINING = 0x2E,
  CMD_RESEND = 0x2F,
  CMD_WRITECOINSUBTRACT = 0x30,
  CMD_WRITEPAYOUT = 0x31,
  CMD_WRITEGPIO1 = 0x32,
  CMD_WRITEANALOG = 0x33,
  CMD_WRITECHAR = 0x34,
  CMD_WRITECOINADDED = 0x35,
  CMD_WRITEPAYOUTSUBTRACT = 0x36,
  CMD_WRITEGPIOBYTE = 0x37,
  CMD_WRITEGPIOBIT = 0x38
};

struct Packet
{
  int address;
  int length;
  byte message[255];
};

struct Reply
{
  int length;
  byte message[255];
};

Packet packet;
Reply reply[2];
int curReply = 0;
int deviceId = -1;
int coin1 = 0;
int coin2 = 0;
//int coin3 = 0;  //for if I need it
//int coin4 = 0;

byte features[] =
{
  #if defined(PLAYER_1)
  1, 1, 12, 0,		// Players=1 Switches=12
  2, 1, 0, 0, 	// 1 coin slot
  //3, 3, 8, 0,   // 3 analog channels, 8-bit each (but Naomi expects 16, so 2 8's per channel)
  0				// End of features
  #elif defined(PLAYER_2)
  1, 2, 12, 0,    // Players=2 Switches=12
  2, 2, 0, 0,   // 2 coin slots (Capcom games seem to require two coin slots for two player mode)
  //3, 3, 8, 0,   // 3 analog channels, 8-bit each (but Naomi expects 16, so 2 8's per channel)
  0       // End of features
  #elif defined(PLAYER_4)
  1, 4, 12, 0,    // Players=4 Switches=12
  2, 2, 0, 0,   // 2 coin slot
  //3, 3, 8, 0,   // 3 analog channels, 8-bit each (but Naomi expects 16, so 2 8's per channel)
  0       // End of features
  #endif
};

byte zeros[64] = {
  0
};
byte tmp[4] = {
  0
};

void setup()
{
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1);
  Serial1.transmitterEnable(PIN_TXENABLE);
  pinMode(PIN_SENSE, INPUT);
  //pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_SENSE, LOW);
  //digitalWrite(PIN_LED, HIGH);
  Serial.println("Charlie's JVS Emulator");
  //pinMode(PIN_COIN_1, INPUT_PULLUP);
  //pinMode(PIN_COIN_2, INPUT_PULLUP);
  //Coin readers seem susceptible to noise.
  coindebouncer1.attach(PIN_COIN_1, INPUT_PULLUP);
  coindebouncer2.attach(PIN_COIN_2, INPUT_PULLUP);
  //init 12 pins per player, 4 players, starting at pin 6
  for (int i = 6; i <= 54; i++)
  {
    pinMode(i, INPUT_PULLUP);
  }
  pinMode(13, OUTPUT);    //can't use this one due to LED
  digitalWrite(13, HIGH);
}

void ReplyBytes(const byte *bytes, int numBytes)
{
  Reply *r = &reply[curReply];
  r->message[r->length++] = 0x01;
  for (int i = 0; i < numBytes; i++)
  {
    r->message[r->length++] = bytes[i];
  }
}

void writeEscaped(byte b)
{
  if (b == 0xE0 || b == 0xD0)
  {
    Serial1.write(0xD0);
    b--;
  }
  Serial1.write(b);
}

void FlushReply()
{
  Reply *r = &reply[curReply];
  if (r->length > 0)
  {
    int sum = 3 + r->length;
    Serial1.write(0xE0);
    writeEscaped(0x00);
    writeEscaped(r->length + 2);
    writeEscaped(0x01);
    for (int i = 0; i < r->length; i++)
    {
      sum += r->message[i];
      writeEscaped(r->message[i]);
    }
    writeEscaped(sum & 0xFF);
    curReply = 1 - curReply;
    reply[curReply].length = 0;
  }
}

void Resend()
{
  Reply *r = &reply[curReply];
  Reply *old = &reply[1 - curReply];
  int length = old->length;
  for (int i = 0; i < length; i++)
  {
    r->message[r->length++] = old->message[i];
  }
}

#define Reply()			 ReplyBytes(NULL,0)
#define ReplyString(str) ReplyBytes((const byte*)str,sizeof(str))
#define ReplyByte(b)	 do { tmp[0]=b; ReplyBytes(tmp, 1); } while(0)

void ProcessPacket(struct Packet *p)
{
  if (p->address == 0xFF || p->address == deviceId) // Yay, it's for me
  {
    if (p->address == 0xFF)
      LOG("Broadcast");
    else
      LOG("To me");
    int length = p->length;
    byte *message = p->message;
    while (length > 0)
    {
      int sz = 1;
      switch (message[0])
      {
        case CMD_RESET:
          LOG("CMD_RESET");
          sz = 2;
          deviceId = -1;
          pinMode(PIN_SENSE, INPUT);
          break;
        case CMD_SETADDRESS:
          LOG("CMD_SETADDRESS");
          sz = 2;
          deviceId = message[1];
          Reply();
          pinMode(PIN_SENSE, OUTPUT);
          break;
        case CMD_SETMETHOD:
          LOG("CMD_SETMETHOD");
          break;

        case CMD_READID:
          LOG("CMD_READID");
          ReplyString("www.github.com/charcole;A Teensy JVS IO Device;Version 1.0;Charlie's JVS emulator"); //Not technically true, but I feel like I should leave credit where due
          break;
        case CMD_FORMATVERSION:
          LOG("CMD_FORMATVERSION");
          ReplyByte(0x13);
          break;
        case CMD_JVSVERSION:
          LOG("CMD_JVSVERSION");
          ReplyByte(0x30);
          break;
        case CMD_COMMSVERSION:
          LOG("CMD_COMMSVERSION");
          ReplyByte(0x10);
          break;
        case CMD_GETFEATURES:
          LOG("CMD_GETFEATURES");
          ReplyBytes(features, sizeof(features));
        case CMD_SETMAINBOARDID:
          LOG("CMD_SETMAINBOARDID");
          sz = strlen((char*)&message[1]) + 1;
          LOG((char*)&message[1]);
          Reply();
          break;

        case CMD_READSWITCHES:
          {
            //message[1] = #players to read data for
            //message[2] = number of bytes to read
            LOG("CMD_READSWITCHES");
            sz = 3;
            byte results[9];  //for 4 players, 2 bytes each.
            for(int i = 0; i < 9; i++)
            {
              results[i] = 0;
            }
            //leave results[0] set to 0
            //players * 12 buttons, players * 2 bytes to send
            //player 1
            results[1] |= !digitalRead(6);   //start
            results[1] = results[1] << 1;
            results[1] |= !digitalRead(7);   //service (exists, but not connected)
            results[1] = results[1] << 1;
            results[1] |= !digitalRead(8);   //up
            results[1] = results[1] << 1;
            results[1] |= !digitalRead(9);   //down
            results[1] = results[1] << 1;
            results[1] |= !digitalRead(10);  //left
            results[1] = results[1] << 1;
            results[1] |= !digitalRead(11);  //right
            results[1] = results[1] << 1;
            results[1] |= !digitalRead(12);  //P1
            results[1] = results[1] << 1;
            //results[1] |= !digitalRead(13);  //P2 //skip 13, LED interferes
            results[1] |= !digitalRead(14);  //P2
            //next byte
            results[2] |= !digitalRead(15);  //P3
            results[2] = results[2] << 1;
            results[2] |= !digitalRead(16);  //P4
            results[2] = results[2] << 1;
            results[2] |= !digitalRead(17);  //P5
            results[2] = results[2] << 1;
            results[2] |= !digitalRead(18);  //P6
            results[2] = results[2] << 4;
            //player 2
            results[3] |= !digitalRead(19);  //start
            results[3] = results[3] << 1;
            results[3] |= !digitalRead(20);  //service (exists, but not connected)
            results[3] = results[3] << 1;
            results[3] |= !digitalRead(21);  //up
            results[3] = results[3] << 1;
            results[3] |= !digitalRead(22);  //down
            results[3] = results[3] << 1;
            results[3] |= !digitalRead(23);  //left
            results[3] = results[3] << 1;
            results[3] |= !digitalRead(24);  //right
            results[3] = results[3] << 1;
            results[3] |= !digitalRead(25);  //P1
            results[3] = results[3] << 1;
            results[3] |= !digitalRead(26);  //P2
            //next byte
            results[4] |= !digitalRead(27);  //P3
            results[4] = results[4] << 1;
            results[4] |= !digitalRead(28);  //P4
            results[4] = results[4] << 1;
            results[4] |= !digitalRead(29);  //P5
            results[4] = results[4] << 1;
            results[4] |= !digitalRead(30);  //P6
            results[4] = results[4] << 4;
#if defined(PLAYER_4)
            //player 3
            results[5] |= !digitalRead(31);
            results[5] = results[5] << 1;
            results[5] |= !digitalRead(32);
            results[5] = results[5] << 1;
            results[5] |= !digitalRead(33);
            results[5] = results[5] << 1;
            results[5] |= !digitalRead(34);
            results[5] = results[5] << 1;
            results[5] |= !digitalRead(35);
            results[5] = results[5] << 1;
            results[5] |= !digitalRead(36);
            results[5] = results[5] << 1;
            results[5] |= !digitalRead(37);
            results[5] = results[5] << 1;
            results[5] |= !digitalRead(38);
            //next byte
            results[6] |= !digitalRead(39);
            results[6] = results[6] << 1;
            results[6] |= !digitalRead(40);
            results[6] = results[6] << 1;
            results[6] |= !digitalRead(41);
            results[6] = results[6] << 1;
            results[6] |= !digitalRead(42);
            results[6] = results[6] << 4;
            //player 4
            results[7] |= !digitalRead(43);
            results[7] = results[7] << 1;
            results[7] |= !digitalRead(44);
            results[7] = results[7] << 1;
            results[7] |= !digitalRead(45);
            results[7] = results[7] << 1;
            results[7] |= !digitalRead(46);
            results[7] = results[7] << 1;
            results[7] |= !digitalRead(47);
            results[7] = results[7] << 1;
            results[7] |= !digitalRead(48);
            results[7] = results[7] << 1;
            results[7] |= !digitalRead(49);
            results[7] = results[7] << 1;
            results[7] |= !digitalRead(50);
            //next byte
            results[8] |= !digitalRead(51);
            results[8] = results[8] << 1;
            results[8] |= !digitalRead(52);
            results[8] = results[8] << 1;
            results[8] |= !digitalRead(53);
            results[8] = results[8] << 1;
            results[8] |= !digitalRead(54);
            results[8] = results[8] << 4;
#endif

            //the original brilliant, and inflexible, read
            /*
            //pin 23 = "P1 start", pin 14 = "P1 push 4"
            int result = 0;
            int mask = 1;
            for (int i = 14; i <= 23; i++)
            {
              if (!digitalRead(i))
                result |= mask;
              mask <<= 1;
            }
            */
            /*
            results[0] = 0;
            results[1] = (result >> 2) & 0xFF;
            results[2] = (result << 6) & 0xFF;
            Serial.println(result, BIN);
            */
            // Simply don't send read results for unused users
            for(int i=0; i < message[1]*message[2] + 1; i++)
            {
              Serial.println(results[i], HEX);
            }
            ReplyBytes(results, message[1]*message[2] + 1);
            break;
          }
        case CMD_READCOIN:
          {
            //two most sig bits are actually status, with 00 being normal operation
            //byte coins3[2] = {(byte)(coin3 >> 8), (byte)coin3};
            //byte coins4[2] = {(byte)(coin4 >> 8), (byte)coin4};
            LOG("CMD_READCOIN");
            sz = 2;
            #if defined(PLAYER_1)
              byte coins[2] = {(byte)(coin1 >> 8), (byte)coin1};
              ReplyBytes(coins, message[1] * 2);
            #elif  defined(PLAYER_2)
              byte coins[4] = {(byte)(coin1 >> 8), (byte)coin1, (byte)(coin2 >> 8), (byte)coin2};
              ReplyBytes(coins, message[1] * 2);
            #endif
            break;
          }
        case CMD_READANALOG:
          {
            uint8_t analog[6] = {0, 0, 0, 0, 0, 0};

            //analog reading code here
            //remember, two bytes per channel
            
            LOG("CMD_READANALOG");
            sz = 2;
            ReplyBytes(analog, message[1] * 2);
            break;
          }
        case CMD_READROTARY:
          LOG("CMD_READROTARY");
          sz = 2;
          ReplyBytes(zeros, message[1] * 2);
          break;
        case CMD_READKEYCODE:
          LOG("CMD_READKEYCODE");
          Reply();
          break;
        case CMD_READSCREENPOS:
          LOG("CMD_READSCREENPOS");
          sz = 2;
          ReplyBytes(zeros, 4);
          break;
        case CMD_READGPIO:
          LOG("CMD_READGPIO");
          sz = 2;
          ReplyBytes(zeros, message[1]);
          break;

        case CMD_WRITEPAYOUTREMAINING:
          LOG("CMD_WRITEPAYOUTREMAINING");
          sz = 2;
          ReplyBytes(zeros, 4);
          break;
        case CMD_RESEND:
          LOG("CMD_RESEND");
          Resend();
          break;
        case CMD_WRITECOINSUBTRACT:
          LOG("CMD_WRITECOINSUBTRACT");
          sz = 4;
          Reply();
          break;
        case CMD_WRITEPAYOUT:
          LOG("CMD_WRITEPAYOUT");
          sz = 4;
          Reply();
          break;
        case CMD_WRITEGPIO1:
          LOG("CMD_WRITEGPIO1");
          sz = 2 + message[1];
          Reply();
          break;
        case CMD_WRITEANALOG:
          LOG("CMD_WRITEANALOG");
          sz = 2 + 2 * message[1];
          Reply();
          break;
        case CMD_WRITECHAR:
          LOG("CMD_WRITECHAR");
          sz = 2 + message[1];
          Reply();
          break;
        case CMD_WRITECOINADDED:
          LOG("CMD_WRITECOINADDED");
          sz = 4;
          Reply();
          break;
        case CMD_WRITEPAYOUTSUBTRACT:
          LOG("CMD_WRITEPAYOUTSUBTRACT");
          sz = 4;
          Reply();
          break;
        case CMD_WRITEGPIOBYTE:
          LOG("CMD_WRITEGPIOBYTE");
          sz = 3;
          Reply();
          break;
        case CMD_WRITEGPIOBIT:
          LOG("CMD_WRITEGPIOBIT");
          sz = 3;
          Reply();
          break;

        default:
          LOG("Got unknown");
          Reply();
          break;
      }
      message += sz;
      length -= sz;
    }
    if (length < 0)
    {
      LOG("Underflowed!");
    }
    FlushReply();
  }
  else
  {
    LOG("Not for me");
  }
}

// Recieving
bool bRecieving = false;
bool bEscape = false;
int phase = 0;
int checksum = 0;
int cur = 0;
bool bAddCoin = false;

void loop()
{
  //if (!digitalRead(PIN_COIN_1) || !digitalRead(PIN_COIN_2))
  //code for debouncing coin inputs.  No need for a single press to yeild 27 credits.
  coindebouncer1.update();
  coindebouncer2.update();
  if (coindebouncer1.fell())
  {
    if (bAddCoin)
    {
      coin1++;
      bAddCoin = false;
    }
  }
  if (coindebouncer2.fell())
  {
    if (bAddCoin)
    {
      coin2++;
      bAddCoin = false;
    }
  }
  else
  {
    bAddCoin = true;
  }
  if (Serial1.available() > 0)
  {
    //digitalWrite(PIN_LED, LOW);
    byte incomingByte = Serial1.read();
    switch (incomingByte)
    {
      case 0xE0: // sync
        bRecieving = true;
        bEscape = false;
        phase = 0;
        cur = 0;
        checksum = 0;
        break;
      case 0xD0: // Escape
        bEscape = true;
        break;
      default:
        if (bEscape)
        {
          incomingByte++;
          bEscape = false;
        }
        switch (phase)
        {
          case 0:
            packet.address = incomingByte;
            checksum += incomingByte;
            phase++;
            break;
          case 1:
            packet.length = incomingByte - 1;
            checksum += incomingByte;
            phase++;
            break;
          case 2:
            if (cur >= packet.length)
            {
              checksum &= 0xFF;
              if (checksum == incomingByte)
              {
                ProcessPacket(&packet);
              }
              else
              {
                LOG("Dropping packet");
              }
              bRecieving = false;
            }
            else
            {
              checksum += incomingByte;
              packet.message[cur++] = incomingByte;
            }
            break;
        }
        break;
    }
    //digitalWrite(PIN_LED, HIGH);
  }
}
