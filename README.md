# RetroStick
Joystick Steuerung für RetroPie

## Anforderungen
[RetroPie](http://blog.petrockblock.com/retropie/) läuft am besten auf einem Raspberry Pi 2.

Um die Joysticksteuerung zu installieren, wird zunächst [WiringPi](http://wiringpi.com/download-and-install/) vorausgesetzt. Da es auf [Adafruit Retro-Game](https://github.com/adafruit/Adafruit-Retrogame) basiert, müssen die dortigen Voraussetzungen ebenfalls erfüllt werden.

## Verkabelung
Die RetroStick.fzz Datei kann mit [Fritzing](http://fritzing.org/download/) geöffnet werden. Bisher habe ich mir noch keine Mühe gemacht, den Schaltplan richtig zu setzen, aber die Übersicht sollte genügend Aufschluss geben.

Beim Münzeinwurf wird der Signal Pin an den Raspberry angeschlossen, die Masse ebenfalls. Ansonsten muss noch die Stromversorgung (12V) verbunden werden. Der Münzzähler ist optional.

Der Monitor wird erst mit dem Hochfahren angeschaltet. Dazu muss ein Relais angeschlossen werden.

Um mit einem anderen Rechner zu spielen, kann der Monitor des Kabinetts mitgenutzt werden und die Steuerung seriell herausgeführt werden. Ein KVM Switch kann dazu installiert werden. Ich habe dafür zwei Relais geopfert:

KVM Switch ON schaltet 5V für den Switch an und aus. KVM Switch Select leitet die 5V an den jeweiligen PS2 Eingang.

Über die serielle Schnittstelle kann etwa ein Arduino Micro oder Leonardo angeschlossen werden. Die Daten werden als #KEY ON/OFF gesendet. So erhält man beim bewegen des Joysticks etwa **2 1** / **2 0** und für das Drücken einer Taste **12 1** / **12 0**. Die Coins werden mit 40 und 41 ausgesandt.

Die genaue Pinbelegung kann im `keys[]` eingesehen werden. Der vorletzte Pin am MCP23017 (GPB6) fungiert als Shift-Taste. Damit werden die Tasten umbelegt, etwa auf Münzeinwurf, Bildschirmwechsel, TAB/ESC oder herunterfahren.


## Installation

Zur Installation [vorher Anforderungen erfüllen!] kann einfach `$ make` und `$ make install` ausgeführt werden. Dadurch wird das Joystick Programm kompiliert, verschoben und der Autostart eingerichtet.
