joystick : joystick.c
	gcc joystick.c -lwiringPi -Wall -O3 -fomit-frame-pointer -funroll-loops -s -o joystick

install:
	mv joystick /usr/local/bin
	cp -f joystick-starter /etc/init.d/joystick
	update-rc joystick defaults

clean:
	rm -f joystick
