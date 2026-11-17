CC = gcc
CFLAGS = -Wall -Wextra -g -pthread

# All source files for the project
SRCS = main.c banker.c dispatch.c ipc.c logger_pthread.c scheduler.c
OBJS = $(SRCS:.c=.o)
TARGET = scheduler.out

.PHONY: all clean

# Default build target
all: $(TARGET)

# Link all object files into the final executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Compile each .c file into a .o object file
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up build artifacts and logs
clean:
	rm -f $(OBJS) $(TARGET) scheduler_events.log