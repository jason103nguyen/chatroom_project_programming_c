compile:
	gcc -Wall -g3 -fsanitize=address -pthread server.c -o server.o
	gcc -Wall -g3 -fsanitize=address -pthread client_tcp.c -o client_tcp.o
	gcc -Wall -g3 -fsanitize=address -pthread client_udp.c -o client_udp.o
FLAGS    = -L /lib64
LIBS     = -lusb-1.0 -l pthread

