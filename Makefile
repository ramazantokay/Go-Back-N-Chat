# Ramazan TOKAY 07.12.2022
# 

all: build

build: server.c client.c
	gcc -pthread -Wall -o server server.c
	gcc -pthread -Wall -o client client.c

clean:
	rm server client