#pragma once
#include "str.h"

typedef struct {
    bool chess960;  // play Chess960
    int concurrency;  // number of concurrent games
    int games;  // number of games
    str_t openings;  // opening set
    bool random;  // start from a random opening
    bool repeat;  // repeat each opening twice with colors reversed
} Options;

Options options_new(int argc, const char **argv);
void options_delete();
