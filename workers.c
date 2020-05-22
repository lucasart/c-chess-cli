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
#include <stdlib.h>
#include "workers.h"

Worker *Workers;
static int WorkersCount;

void workers_new(int count)
{
    WorkersCount = count;
    Workers = calloc(count, sizeof(Worker));

    for (int i = 0; i < WorkersCount; i++)
        Workers[i].id = i;
}

void workers_delete()
{
    free(Workers);
    Workers = NULL;
    WorkersCount = 0;
}

void workers_total(int wld[3])
{
    wld[0] = wld[1] = wld[2] = 0;

    for (int i = 0; i < WorkersCount; i++)
        for (int j = 0; j < 3; j++)
            wld[j] += Workers[i].wld[j];
}
