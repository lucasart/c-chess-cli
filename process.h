#pragma once
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

typedef struct {
    pid_t pid;
    FILE *in, *out;
} Process;

bool process_create(Process *p, const char *cmd);
bool process_terminate(Process *p);

bool process_readln(const Process *p, char *buf, size_t n);
bool process_writeln(const Process *p, char *buf);
