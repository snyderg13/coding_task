CC     = gcc
CFLAGS = -Wall -Wextra -Werror -std=c11 -g \
         -fsanitize=address,undefined

TARGET = test_parser
SRCS   = parser.c test_parser.c

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRCS) parser.h
	$(CC) $(CFLAGS) -o $@ $(SRCS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
