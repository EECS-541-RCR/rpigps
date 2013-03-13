main: main.o
	g++ -O3 -Wall -g -lpthread -lgps -o main main.o

main.o: main.c
	g++ -O3 -Wall -g -lpthread -lgps -c main.c

clean:
	rm -f main *.o

