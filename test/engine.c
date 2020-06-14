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
// Stand alone program: minimal UCI engine (random mover) used for testing and benchmarking
#include "gen.h"

#define uci_printf(...) printf(__VA_ARGS__), fflush(stdout)
#define uci_puts(str) puts(str), fflush(stdout)

void parse_position(const char *tail, Position *pos)
{
    scope(str_del) str_t token = (str_t){0};

    tail = str_tok(tail, &token, " ");
    assert(tail);

    if (!strcmp(tail, "startpos"))
        pos_set(pos, str_ref("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"));
    else if (!strcmp(tail, "fen")) {
        scope(str_del) str_t fen = (str_t){0};

        while ((tail = str_tok(tail, &token, " ")) && strcmp(token.buf, "moves"))
            str_push(str_cat(&fen, token), ' ');

        Position p[2];
        int ply = 0;
        pos_set(&p[ply], fen);

        if (!strcmp(token.buf, "moves")) {
            while ((tail = str_tok(tail, &token, " ")))
                pos_move(&p[(ply + 1) % 2], &p[ply % 2], pos_lan_to_move(&p[ply % 2], token, false));

            *pos = p[(ply + 1) % 2];
        }
    } else
        assert(false);
}

int main(void)
{
    Position pos;
    scope(str_del) str_t line = (str_t){0}, token = (str_t){0};

    while (str_getline(&line, stdin)) {
        const char *tail = str_tok(line.buf, &token, " ");

        if (!strcmp(token.buf, "uci"))
            uci_puts("uciok");
        else if (!strcmp(token.buf, "isready"))
            uci_puts("readyok");
        else if (!strcmp(token.buf, "position"))
            parse_position(tail, &pos);
        else if (!strcmp(token.buf, "quit"))
            break;
    }
}
