#define VERBOSE
#define NODEAMON
/*
ADAFRUIT RETROGAME UTILITY: remaps buttons on Raspberry Pi GPIO header
to virtual USB keyboard presses.  Great for classic game emulators!
Retrogame is interrupt-driven and efficient (usually under 0.3% CPU use)
and debounces inputs for glitch-free gaming.

Connect one side of button(s) to GND pin (there are several on the GPIO
header, but see later notes) and the other side to GPIO pin of interest.
Internal pullups are used; no resistors required.  Avoid pins 8 and 10;
these are configured as a serial port by default on most systems (this
can be disabled but takes some doing).  Pin configuration is currently
set in global table; no config file yet.  See later comments.

Must be run as root, i.e. 'sudo ./retrogame &' or configure init scripts
to launch automatically at system startup.

Requires uinput kernel module.  This is typically present on popular
Raspberry Pi Linux distributions but not enabled on some older varieties.
To enable, either type:

    sudo modprobe uinput

Or, to make this persistent between reboots, add a line to /etc/modules:

    uinput

To use with the Cupcade project (w/Adafruit PiTFT and menu util), retrogame
must be recompiled with CUPCADE #defined, i.e.:

    make clean; make CFLAGS=-DCUPCADE

Written by Phil Burgess for Adafruit Industries, distributed under BSD
License.  Adafruit invests time and resources providing this open source
code, please support Adafruit and open-source hardware by purchasing
products from Adafruit!


Copyright (c) 2013 Adafruit Industries.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <sys/types.h>

#include <wiringPi.h>
#include <mcp23017.h>
#include <mcp23008.h>
#include <wiringPiI2C.h>
#include <wiringSerial.h>


int key_state[24];
int keys[] = {
		KEY_F,          //P2 Down
		KEY_G,          //P2 Right
		KEY_R,          //P2 Up
		KEY_D,          //P2 Left
		KEY_DOWN,       //P1 Down
		KEY_RIGHT,      //P1 Right
		KEY_UP,         //P1 Up
		KEY_LEFT,       //P1 Left
		KEY_A,          //P2 1
		KEY_S,          //P2 2
		KEY_Q,          //P2 3
		KEY_W,          //P2 4
		KEY_LEFTCTRL,   //P1 1
		KEY_LEFTALT,    //P1 2
		KEY_SPACE,      //P1 3
		KEY_LEFTSHIFT,  //P1 4
		KEY_Y,          //P1 5
		KEY_X,          //P1 6
		KEY_I,          //P2 5
		KEY_K,          //P2 6
		KEY_1,          //1 Player
		KEY_2,          //2 Player
		KEY_RESERVED,   //SHIFT
		KEY_RESERVED,   //NC
		//SHIFTED BUTTONS
		KEY_RESERVED,   //Quit this program
		KEY_RESERVED,   //None
		KEY_RESERVED,   //None
		KEY_RESERVED,   //None
		KEY_ENTER,      //Enter
		KEY_5,          //Coin 1
		KEY_P,          //P
		KEY_F4,         //F4
		KEY_RESERVED,   //None
		KEY_RESERVED,   //None
		KEY_RESERVED,   //None
		KEY_RESERVED,   //None
		KEY_TAB,        //MAME Settings
		KEY_ESC,        //Quit Game
		//COIN MECH
		KEY_5,          //Coin 1
		KEY_6,          //Coin 2
};
#define KEYLEN (sizeof(keys) / sizeof(keys[0])) // io[] table size

int coin[2],shift;
int udevfd;
int serial;
struct input_event keyEv,synEv;

// A few globals ---------------------------------------------------------

char *progName;                         // Program name (for error reporting)
volatile char running = 1;                 // Signal handler will set to 0 (exit)
char externalDisplay = 0;
const int
   debounceTime = 20;                // 20 ms for button debouncing
int mcp08fd,mcp17fd;


// Quick-n-dirty error reporter; print message, clean up and exit.
void err(char *msg) {
	printf("%s: %s.  Try 'sudo %s'.\n", progName, msg, progName);
	exit(1);
}

#define COIN1	7
#define COIN2	3
#define MCP08	0
#define MCP17	2
#define MCP08PINBASE 100
#define MCP17PINBASE 108
#define SCREENON 6
#define SCREENSWITCHON 10
#define SCREENSWITCH 11

#define MCP23017_IODIRA 0x00
#define MCP23017_IPOLA 0x02
#define MCP23017_GPINTENA 0x04
#define MCP23017_DEFVALA 0x06
#define MCP23017_INTCONA 0x08
#define MCP23017_IOCONA 0x0A
#define MCP23017_GPPUA 0x0C
#define MCP23017_INTFA 0x0E
#define MCP23017_INTCAPA 0x10
#define MCP23017_GPIOA 0x12
#define MCP23017_OLATA 0x14


#define MCP23017_IODIRB 0x01
#define MCP23017_IPOLB 0x03
#define MCP23017_GPINTENB 0x05
#define MCP23017_DEFVALB 0x07
#define MCP23017_INTCONB 0x09
#define MCP23017_IOCONB 0x0B
#define MCP23017_GPPUB 0x0D
#define MCP23017_INTFB 0x0F
#define MCP23017_INTCAPB 0x11
#define MCP23017_GPIOB 0x13
#define MCP23017_OLATB 0x15

#define MCP23008_IODIR 0x00
#define MCP23008_IPOL 0x01
#define MCP23008_GPINTEN 0x02
#define MCP23008_DEFVAL 0x03
#define MCP23008_INTCON 0x04
#define MCP23008_IOCON 0x05
#define MCP23008_GPPU 0x06
#define MCP23008_INTF 0x06
#define MCP23008_INTCAP 0x08
#define MCP23008_GPIO 0x09
#define MCP23008_OLAT 0x0A

void setScreen(char display) {
	externalDisplay = display;
	digitalWrite(SCREENSWITCH, !externalDisplay);
	digitalWrite(SCREENSWITCHON, 0);
	sleep(1);
	digitalWrite(SCREENSWITCHON, 1);
}

void endOfKeyEvent(void) {
	write(udevfd, &synEv, sizeof(synEv));
}
void keyEvent(int key, int pressed) {
	keyEv.code  = key;
	keyEv.value = pressed;
	#ifdef VERBOSE
	printf("KEY %d : %d\n",key,pressed);
	#endif
	write(udevfd, &keyEv, sizeof(synEv));
}

void serialEvent(int key, int state) {
	serialPutchar(serial,key);
	serialPutchar(serial,state);
}

void coin1isr(void) {
	int tmp = !digitalRead(COIN1);
	#ifdef VERBOSE
	printf("COIN 1 Interrupt %d\n",tmp);
	#endif
	if (tmp != coin[0]) {
		if (externalDisplay) {
			serialEvent(40,tmp);
		} else {
			keyEvent(KEY_5, tmp);
			endOfKeyEvent();
		}
	}
	coin[0] = tmp;
}
void coin2isr(void) {
	int tmp = !digitalRead(COIN2);
	#ifdef VERBOSE
	printf("COIN 2 Interrupt\n");
	#endif
	if (tmp != coin[1]) {
		if (externalDisplay) {
			serialEvent(41,tmp);
		} else {
			keyEvent(KEY_6, tmp);
			endOfKeyEvent();
		}
	}
	coin[1] = tmp;
}
void mcp08isr(void) {
	int readVal,tmp,change;
	int i;
	readVal = wiringPiI2CReadReg8(mcp08fd,MCP23008_GPIO);
	#ifdef VERBOSE
	printf("MCP23008 Interrupt\n");
	#endif
	change = 0;
	for (i=0; i<8; i++) {
		tmp = (readVal>>i)&1;
		if (tmp != key_state[i]) {
			if (externalDisplay) {
				serialEvent(i,tmp);
			} else {
				keyEvent(keys[i],tmp);
				change = 1;
			}
		}
		key_state[i] = tmp;
	}
	if (change) {
		endOfKeyEvent();
	}
}
void mcp17isr(void) {
	int readVal,tmp,change;
	int i,j;
	readVal = wiringPiI2CReadReg8(mcp17fd,MCP23017_GPIOB);
	readVal <<= 8;
	readVal |= wiringPiI2CReadReg8(mcp17fd,MCP23017_GPIOA);
	#ifdef VERBOSE
	printf("MCP23017 Interrupt\n");
	#endif
	for (i=0; i<16; i++) {
		tmp = (readVal>>i)&1;
		if (tmp != key_state[i+8]) {
			if (i == 14) {//SHIFT
				shift = tmp;
				for (j = 0; j < KEYLEN; j++) {
					if (key_state[i]) {
						keyEvent(keys[i],0);
						key_state[i] = 0;
					}
				}
			} else if (!externalDisplay) {
				keyEvent(keys[i+8+16*shift],tmp);
			} else if (!shift && externalDisplay) {
				serialEvent(i+8,tmp);
			}
			if (shift && (i == 11)) {
				digitalWrite(SCREENON,0);
				system("sudo /sbin/halt &");
			} else if (shift && tmp && (i == 10)) {
				setScreen(!externalDisplay);
			}
			change = 1;
			key_state[i+8] = tmp;
		}
	}
	if (change) {
		endOfKeyEvent();
	}
}

void mcpSetup() {
	int ioconfValue = 0;
	ioconfValue=wiringPiI2CReadReg8(mcp08fd,MCP23008_IOCON);
	ioconfValue |= 6;
	wiringPiI2CWriteReg8(mcp08fd,MCP23008_IOCON,ioconfValue);
	ioconfValue=wiringPiI2CReadReg8(mcp17fd,MCP23017_IOCONA);
	ioconfValue |= 70;
	wiringPiI2CWriteReg8(mcp17fd,MCP23017_IOCONA,ioconfValue);
	ioconfValue=wiringPiI2CReadReg8(mcp17fd,MCP23017_IOCONB);
	ioconfValue |= 70;
	wiringPiI2CWriteReg8(mcp17fd,MCP23017_IOCONB,ioconfValue);
	wiringPiI2CWriteReg8(mcp08fd,MCP23008_IODIR,255); //Inputs
	wiringPiI2CWriteReg8(mcp17fd,MCP23017_IODIRA,255);
	wiringPiI2CWriteReg8(mcp17fd,MCP23017_IODIRB,255);
	wiringPiI2CWriteReg8(mcp08fd,MCP23008_IPOL,255); //Invert logic
	wiringPiI2CWriteReg8(mcp17fd,MCP23017_IPOLA,255);
	wiringPiI2CWriteReg8(mcp17fd,MCP23017_IPOLB,255);
	wiringPiI2CWriteReg8(mcp08fd,MCP23008_GPPU,255); //Pullup
	wiringPiI2CWriteReg8(mcp17fd,MCP23017_GPPUA,255);
	wiringPiI2CWriteReg8(mcp17fd,MCP23017_GPPUB,255);
	wiringPiI2CWriteReg8(mcp08fd,MCP23008_INTCON,0); //Interrupt on Change
	wiringPiI2CWriteReg8(mcp17fd,MCP23017_INTCONA,0);
	wiringPiI2CWriteReg8(mcp17fd,MCP23017_INTCONB,0);
	wiringPiI2CWriteReg8(mcp08fd,MCP23008_GPINTEN,255); //Enable Interrupts
	wiringPiI2CWriteReg8(mcp17fd,MCP23017_GPINTENA,255);
	wiringPiI2CWriteReg8(mcp17fd,MCP23017_GPINTENB,255);
	#ifdef VERBOSE
	ioconfValue = wiringPiI2CReadReg8(mcp08fd,MCP23008_GPIO);
	printf("GPIO STATE 08: %d\n",ioconfValue);
	ioconfValue = wiringPiI2CReadReg8(mcp17fd,MCP23017_GPIOA);
	printf("GPIO STATE 17A: %d\n",ioconfValue);
	ioconfValue = wiringPiI2CReadReg8(mcp17fd,MCP23017_GPIOB);
	printf("GPIO STATE 17B: %d\n",ioconfValue);
	#endif
}


int main(int argc, char *argv[]) {
	int i;
	#ifndef NODEAMON
	int fd;
	pid_t pid;
	#endif
	struct uinput_user_dev uidev;

	wiringPiSetup();
	mcp08fd = wiringPiI2CSetup(0x20);
	mcp17fd = wiringPiI2CSetup(0x21);

	pinMode(COIN1, INPUT);
	pinMode(COIN2, INPUT);
	pinMode(MCP08, INPUT);
	pinMode(MCP17, INPUT);
	pinMode(SCREENON, OUTPUT);
	digitalWrite(SCREENSWITCH, 1);
	digitalWrite(SCREENSWITCHON, 1);
	pinMode(SCREENSWITCH, OUTPUT);
	pinMode(SCREENSWITCHON, OUTPUT);
	pullUpDnControl(COIN1, PUD_UP);
	pullUpDnControl(COIN2, PUD_UP);
	pullUpDnControl(MCP08, PUD_UP);
	pullUpDnControl(MCP17, PUD_UP);
	mcpSetup();
	serial = serialOpen("/dev/ttyAMA0",115200);

	progName = argv[0];

	if((udevfd = open("/dev/uinput", O_WRONLY | O_NONBLOCK)) < 0)
		err("Can't open /dev/uinput");
	if(ioctl(udevfd, UI_SET_EVBIT, EV_KEY) < 0)
		err("Can't SET_EVBIT");
	for(i=0; i<40; i++) {
		if(keys[i] != KEY_RESERVED) {
			if(ioctl(udevfd, UI_SET_KEYBIT, keys[i]) < 0)
				err("Can't SET_KEYBIT");
		}
	}
	if(ioctl(udevfd, UI_SET_KEYBIT, KEY_ESC) < 0) err("Can't SET_KEYBIT");
	memset(&uidev, 0, sizeof(uidev));
	snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "retrogame");
	uidev.id.bustype = BUS_USB;
	uidev.id.vendor  = 0x1;
	uidev.id.product = 0x1;
	uidev.id.version = 1;
	if(write(udevfd, &uidev, sizeof(uidev)) < 0)
		err("write failed");
	if(ioctl(udevfd, UI_DEV_CREATE) < 0)
		err("DEV_CREATE failed");

	memset(&keyEv, 0, sizeof(keyEv));
	keyEv.type  = EV_KEY;
	memset(&synEv, 0, sizeof(synEv));
	synEv.type  = EV_SYN;
	synEv.code  = SYN_REPORT;
	synEv.value = 0;

	#ifndef NODEAMON
	pid = fork();
	if (pid < 0) {
		exit(EXIT_FAILURE);
	}
	if (pid > 0) {
		char buf[6];
		sprintf(buf,"%d\n",pid);
		fd = open("/var/run/joystick.pid",O_RDWR|O_CREAT, S_IRGRP|S_IRUSR|S_IWUSR);
		write(fd,buf,strlen(buf)+1);
		close(fd);
		exit(EXIT_SUCCESS);
	}
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);
	#endif

	setScreen(0);
	digitalWrite(SCREENON,1);
	wiringPiISR(COIN1,INT_EDGE_BOTH,coin1isr);
	wiringPiISR(COIN2,INT_EDGE_BOTH,coin2isr);
	while (running) {
		if (!digitalRead(MCP08)) {
			mcp08isr();
		}
		if (!digitalRead(MCP17)) {
			mcp17isr();
		}
		delay(5);
	}
	printf("Stopping Joystick.\n");
	ioctl(udevfd, UI_DEV_DESTROY);
	close(udevfd);
	return 0;
}
