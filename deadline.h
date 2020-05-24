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
#include "engine.h"

typedef struct {
    pthread_mutex_t mtx;
    const Engine *engine;
    int64_t timeLimit;
} Deadline;

void deadline_set(Deadline *deadline, const Engine *engine, int64_t timeLimit);
void deadline_clear(Deadline *deadline);

const Engine *deadline_overdue(Deadline *deadline);
