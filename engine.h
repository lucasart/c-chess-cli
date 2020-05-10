#pragma once
#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>
#include "str.h"

// Engine process
typedef struct {
    pid_t pid;
    FILE *in, *out, *log;
    str_t name;
} Engine;

Engine engine_create(const char *cmd, FILE *log, const char *uciOptions);
void engine_delete(Engine *e);

void engine_readln(const Engine *e, str_t *line);
void engine_writeln(const Engine *e, char *buf);

void engine_sync(const Engine *e);
str_t engine_bestmove(const Engine *e);
