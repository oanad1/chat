CC = g++
CFLAGS = -Wall -g

PORT = 8080
IP_SERVER = 127.0.0.1

ifeq (run_server,$(firstword $(MAKECMDGOALS)))
  SERVER_ARGS := $(wordlist 2,$(words $(MAKECMDGOALS)),$(MAKECMDGOALS))
  $(eval $(SERVER_ARGS):;@:)
endif

ifeq (run_subscriber,$(firstword $(MAKECMDGOALS)))
  SUBSCRIBER_ARGS := $(wordlist 2,$(words $(MAKECMDGOALS)),$(MAKECMDGOALS))
  $(eval $(SUBSCRIBER_ARGS):;@:)
endif

all: server subscriber

server: server.cpp
	$(CC) $(CFLAGS) -o $@ $^

subscriber: subscriber.cpp
	$(CC) $(CFLAGS) -o $@ $^

.PHONY: clean run_server run_subscriber

run_server:   
	./server $(PORT)
  #./server $(SERVER_ARGS) - To run the server with user arguments

run_subscriber:
	./subscriber $(SUBSCRIBER_ARGS) $(IP_SERVER) $(PORT)
  #./subscriber $(SUBSCRIBER_ARGS) - To run the server with user arguments

clean:
	rm -f server subscriber
