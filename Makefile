# Compiler
CC = g++

# Compiler flags
CFLAGS = -Wall -Wextra -pedantic -std=c++17 -I/usr/include/libxml2

# Libraries
LIBS = -lcurl -lxml2

# Source files
SRCS = main.cpp

# Target executable
TARGET = main

# build with OpenMP
omp: CFLAGS += -fopenmp
omp:
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS) $(LIBS)

# Build without OpenMP
noomp:
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS) $(LIBS)

# Clean up build files
clean:
	rm -f $(TARGET)