main: main.o network.o command.o
	gcc -O3 -Wall -g -lpthread -o main main.o network.o command.o

main.o: main.c
	gcc -O3 -Wall -g -lpthread -c main.c

network.o: network.c
	gcc -O3 -Wall -g -c network.c

command.o: command.c
	gcc -O3 -Wall -g -c command.c

usbgps: usbgps.c
	gcc -O3 -Wall -g -lpthread usbgps.c -o usbgps

dummyserver: dummyserver.c
	gcc -O3 -Wall -g -o dummyserver dummyserver.c

dummyclient: dummyclient.c
	gcc -O3 -Wall -g -o dummyclient dummyclient.c

clean:
	rm -f main usbgps dummyserver dummyclient *.o

