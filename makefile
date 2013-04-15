main: main.o network.o command.o gpsutil.o navdata.o
	gcc -Wall -g -o main main.o network.o command.o gpsutil.o navdata.o -lm -lpthread 

main.o: main.c
	gcc -Wall -g -lpthread -c main.c

network.o: network.c
	gcc -Wall -g -c network.c

command.o: command.c
	gcc -Wall -g -c command.c

gpsutil.o: gpsutil.c
	gcc -Wall -g -lm -c gpsutil.c

navdata.o: navdata.c
	gcc -Wall -g -c navdata.c

usbgps: usbgps.c
	gcc -Wall -g usbgps.c -o usbgps -lpthread

dummyserver: dummyserver.c
	gcc -Wall -g -o dummyserver dummyserver.c

dummyclient: dummyclient.c
	gcc -Wall -g -o dummyclient dummyclient.c

clean:
	rm -f main usbgps dummyserver dummyclient *.o

