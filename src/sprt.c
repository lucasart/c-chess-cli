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
#include <math.h>
#include "sprt.h"
#include "workers.h"

static double elo_to_score(double elo)
{
    return 1 / (1 + exp(-elo * log(10) / 400));
}

void sprt_bounds(double alpha, double beta, double *lbound, double *ubound)
// Standard SPRT stuff
{
    *lbound = log(beta / (1 - alpha));
    *ubound = log((1 - beta) / alpha);
}

double sprt_llr(int wld[3], double elo0, double elo1)
// Uses GPSRT approximation by Michel Van Den Bergh:
// http://hardy.uhasselt.be/Toga/GSPRT_approximation.pdf
{
    if (!!wld[0] + !!wld[1] + !!wld[2] < 2)  // at least 2 among 3 must be non zero
        return 0;

    const int n = wld[RESULT_WIN] + wld[RESULT_LOSS] + wld[RESULT_DRAW];
    const double w = (double)wld[RESULT_WIN] / n, l = (double)wld[RESULT_LOSS] / n, d = 1 - w - l;
    const double s = w + d / 2, var = (w + d / 4) - s * s;
    const double s0 = elo_to_score(elo0), s1 = elo_to_score(elo1);

    return (s1 - s0) * (2 * s - s0 - s1) / (2 * var / n);
}
