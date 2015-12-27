run: build
	./server 4 1234

build: prethreads_chatserver.o
	gcc -o server prethreads_chatserver.o -pthread

compile:prethreads_chatserver.c
	gcc -c prethreads_chatserver.c

clean:
	rm server prethreads_chatserver.o
