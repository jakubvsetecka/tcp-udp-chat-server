# Define compiler
CXX=g++
CXXFLAGS=-Wall -std=c++20

# Define all targets
all: client

# Compile the client program
client: client.cpp
	$(CXX) $(CXXFLAGS) client.cpp -o ./bin/client

# Start a server listening on port 8888 using netcat
start_server:
	@echo "Starting server on port 8080..."
	@nc -l 127.0.0.1 8080 &

# Run the client program to send a message
send_message: main
	@./bin/main

run:
	@$(CXX) $(CXXFLAGS) main.cpp -o ./bin/main
	@./bin/main

# Clean up compiled binaries
clean:
	rm -f ./bin/*

.PHONY: all start_server send_message clean

