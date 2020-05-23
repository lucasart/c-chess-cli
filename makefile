CC = musl-clang  # gcc, musl-gcc, clang, musl-clang
OUT = c-chess-cli
debug = no
static = no

# Compilation flags
CF = -std=gnu99 -mpopcnt -O3 -flto -Wfatal-errors -Wall -Wextra -Wstrict-prototypes
# Additional (clang only): -Wshorten-64-to-32 -Wclass-varargs -Wmissing-prototypes
LF = -lpthread

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
