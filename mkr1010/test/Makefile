CFLAGS = -std=c11 -Wall -Wextra -Werror -I../src/espnow_proto -I../src/bsp

.PHONY: test clean
test: test_proto test_sched
	./test_proto
	./test_sched

test_proto: test_proto.c ../src/espnow_proto/espnow_proto.c ../src/espnow_proto/espnow_proto.h
	gcc $(CFLAGS) -o $@ test_proto.c ../src/espnow_proto/espnow_proto.c

test_sched: test_sched.c ../src/bsp/bsp_sched.c ../src/bsp/bsp_sched.h
	gcc $(CFLAGS) -o $@ test_sched.c ../src/bsp/bsp_sched.c

clean:
	rm -f test_proto test_sched
