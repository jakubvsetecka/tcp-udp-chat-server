# Define compiler
CXX=g++
CXXFLAGS=-Wall -std=c++20

# Define all targets
all: main

# Compile the main program
main: main.cpp
	$(CXX) $(CXXFLAGS) main.cpp -o ./bin/main

# Start a server listening on port 8080 using netcat
# -u for UDP
start_server:
	@echo "Starting server on port 8080..."
	@nc -l 127.0.0.1 8080 &

# Run the main program to send a message
send_message: main
	@./bin/main

run:
	@$(CXX) $(CXXFLAGS) main.cpp -o ./bin/main
	@./bin/main

# Clean up compiled binaries
clean:
	rm -f ./bin/*

.PHONY: all start_server send_message clean
