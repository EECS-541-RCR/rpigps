main: main.o network.o command.o
	gcc -O3 -Wall -g -lpthread -lgps -o main main.o network.o command.o

main.o: main.c
	gcc -O3 -Wall -g -lpthread -lgps -c main.c

network.o: network.c
	gcc -O3 -Wall -g -c network.c

command.o: command.c
	gcc -O3 -Wall -g -c command.c

dummyserver: dummyserver.c
	gcc -O3 -Wall -g -o dummyserver dummyserver.c

dummyclient: dummyclient.c
	gcc -O3 -Wall -g -o dummyclient dummyclient.c

clean:
	rm -f main dummyserver dummyclient *.o

