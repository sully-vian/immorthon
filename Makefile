# Compiler
CC = g++

# Compiler flags
CFLAGS = -Wall -Wextra -pedantic -std=c++20 -fopenmp -I/usr/include/libxml2

# Libraries
LIBS = -lcurl -lxml2

# Target executable
TARGET = main

# Source files
SRCS = main.cpp

# Build target
all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET) $(LIBS)

# Clean up build files
clean:
	rm -f $(TARGET)