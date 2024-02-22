# Define compiler
CXX=g++
CXXFLAGS=-Wall -std=c++11

# Define all targets
all: client

# Compile the client program
client: client.cpp
	$(CXX) $(CXXFLAGS) client.cpp -o ./bin/client

# Start a server listening on port 8888 using netcat
start_server:
	@echo "Starting server on port 8888..."
	@nc -l 8888 &

# Run the client program to send a message
send_message: client
	@./bin/client

# Clean up compiled binaries
clean:
	rm -f ./bin/client

.PHONY: all start_server send_message clean

