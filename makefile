CC = musl-clang
OUT = c-chess-cli
debug = no
static = no

# Compilation flags
CF = -std=gnu99 -mpopcnt -O3 -flto -Wfatal-errors -Wall -Wextra -Wshadow
LF =

ifeq ($(debug),no)
	CF += -DNDEBUG
	LF += -s
else
	CF += -g
endif

ifeq ($(static),yes)
	LF += -static
endif

default:
	$(CC) $(CF) ./*.c -o $(OUT) $(LF)

clean:
	rm $(OUT)
