#pragma once
#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>
#include "str.h"

enum {MAX_NAME_CHAR = 64};

// Engine process
typedef struct {
    pid_t pid;
    FILE *in, *out, *log;
    char name[MAX_NAME_CHAR];
} Engine;

void engine_load(Engine *e, const char *cmd, FILE *log);
void engine_kill(Engine *e);

void engine_readln(const Engine *e, str_t *line);
void engine_writeln(const Engine *e, char *buf);

void engine_sync(const Engine *e);
void engine_bestmove(const Engine *e, char *str);
