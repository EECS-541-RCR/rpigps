main: main.o dummyserver.o dummyclient.o
	gcc -O3 -Wall -g -lpthread -lgps -o main main.o
	gcc -O3 -Wall -g -o dummyserver dummyserver.o
	gcc -O3 -Wall -g -o dummyclient dummyclient.o

main.o: main.c
	gcc -O3 -Wall -g -lpthread -lgps -c main.c

dummyserver.o: dummyserver.c
	gcc -O3 -Wall -g -c dummyserver.c

dummyclient.o: dummyclient.c
	gcc -O3 -Wall -g -c dummyclient.c

clean:
	rm -f main dummyserver dummyclient *.o

