CC = gcc
CFLAGS = -Wall -Wextra -g

all: city_manager

city_manager: city_manager.c
	$(CC) $(CFLAGS) -o city_manager city_manager.c

clean:
	rm -f city_manager
	rm -rf downtown Balcescu  # or any test districts