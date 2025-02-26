FLAGS = -g    \
        -Wall \
        -std=c99

SRC := writer.c
TARGET = writer
CC = aarch64-none-linux-gnu-gcc
OBJS := $(SRC:.c=.o)

all: $(TARGET)

$(TARGET) : $(OBJS)
	$(CC) $(FLAGS) $(INCLUDES) $(OBJS) -o $(TARGET) $(LDFLAGS)

clean:
	-rm -f *.o $(TARGET) *.elf *.map