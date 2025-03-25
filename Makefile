
all:
	$(CC) -O2 test_time_consumption.c -o test_time_consumption

clean:
	rm -f test_time_consumption
