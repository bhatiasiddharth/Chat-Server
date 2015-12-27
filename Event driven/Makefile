run: build
	./server 1234

build: eventdriven_chatserver.o
	gcc -pthread eventdriven_chatserver.o -o server

compile:eventdriven_chatserver.c
	gcc -c eventdriven_chatserver.c

clean:
	rm server eventdriven_chatserver.o
