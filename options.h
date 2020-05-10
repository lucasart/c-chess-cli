#pragma once
#include "str.h"

typedef struct {
    bool chess960;  // play Chess960
    int concurrency;  // number of concurrent games
    int games;  // number of games
    str_t openings;  // openings (EPD file)
    str_t pgnout;  // pgn output file
    str_t uciOptions;  // UCI options (eg. "Hash=16,Threads=8")
    bool random;  // start from a random opening
    bool repeat;  // repeat each opening twice with colors reversed
    bool debug;  // log all I/O with engines
} Options;

typedef struct {
    str_t cmd;
} EngineOptions;

Options options_new(int argc, const char **argv, int start);
void options_delete();
