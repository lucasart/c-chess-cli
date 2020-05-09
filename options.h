#pragma once
#include <stdbool.h>

typedef struct {
    bool chess960;  // play Chess960
    int concurrency;  // number of concurrent games
    int games;  // number of games
    const char *openings;  // opening set
    bool random;  // start from a random opening
    bool repeat;  // repeat each opening twice with colors reversed
} Options;

Options options_parse(int argc, const char **argv);
