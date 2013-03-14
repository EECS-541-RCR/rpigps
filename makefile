main: main.o dummyserver.o
	gcc -O3 -Wall -g -lpthread -lgps -o main main.o
	gcc -O3 -Wall -g -o dummyserver dummyserver.o

main.o: main.c
	gcc -O3 -Wall -g -lpthread -lgps -c main.c

dummyserver.o: dummyserver.c
	gcc -O3 -Wall -g -c dummyserver.c

clean:
	rm -f main dummyserver  *.o

