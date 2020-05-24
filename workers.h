/*
 * c-chess-cli, a command line interface for UCI chess engines. Copyright 2020 lucasart.
 *
 * c-chess-cli is free software: you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * c-chess-cli is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
 * even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program. If
 * not, see <http://www.gnu.org/licenses/>.
*/
#pragma once
#include <pthread.h>
#include "engine.h"

// Per thread data
typedef struct {
    Deadline deadline;
    int id;  // starts at 0
    int wldCount[3];  // counts wins, losses, and draws
} Worker;

extern Worker *Workers;
extern _Atomic(int) WorkersBusy;  // how many workers are busy

void workers_new(int count);
void workers_delete(void);

void workers_add_result(Worker *worker, int result, int wld[3]);

void workers_busy_add(int n);
int workers_busy_count(void);
