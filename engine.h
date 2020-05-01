#pragma once
#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>

enum {MAX_LINE_CHAR = 1024};

// Engine process
typedef struct {
    pid_t pid;
    FILE *in, *out, *log;
    char name[64];
} Engine;

bool engine_start(Engine *e, const char *cmd, FILE *log);
bool engine_stop(Engine *e);

bool engine_readln(const Engine *e, char *buf, size_t n);
bool engine_writeln(const Engine *e, char *buf);

void engine_sync(const Engine *e);
void engine_bestmove(const Engine *e, char *str);
