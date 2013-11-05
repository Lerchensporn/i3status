all:
	gcc -o i3status `pkg-config --libs --cflags alsa libmpdclient libnotify` i3status.c
