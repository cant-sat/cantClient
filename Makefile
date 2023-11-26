libs=-lm -lwebsockets

main:
	gcc -o client main.c -I. -lwebsockets
