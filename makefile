all: threadpool.c threadpool.h server.c
	gcc -g threadpool.c server.c -o runfile -lpthread
all-GDB: threadpool.c threadpool.h server.c
	gcc -g threadpool.c server.c -o runfile -lpthread