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
#include "util.h"
#include "vec.h"
#include <string.h>

#define uci_printf(...) printf(__VA_ARGS__), fflush(stdout)
#define uci_puts(str) puts(str), fflush(stdout)

typedef struct {
    int depth;
} Go;

static uint64_t hash_mix(uint64_t block) {
    block ^= block >> 23;
    block *= 0x2127599bf4325c37ULL;
    return block ^= block >> 47;
}

static void hash_block(uint64_t block, uint64_t *hash) {
    *hash ^= hash_mix(block);
    *hash *= 0x880355f21e6d1965ULL;
}

// Based on FastHash64, without length hashing, to make it capable of incremental updates
static void hash_blocks(const void *buffer, size_t length, uint64_t *hash) {
    assert((uintptr_t)buffer % 8 == 0);
    const uint64_t *blocks = (const uint64_t *)buffer;

    for (size_t i = 0; i < length / 8; i++)
        hash_block(*blocks++, hash);

    if (length % 8) {
        const uint8_t *bytes = (const uint8_t *)blocks;
        uint64_t block = 0;

        for (size_t i = 0; i < length % 8; i++)
            block = (block << 8) | *bytes++;

        hash_block(block, hash);
    }
}

static void parse_position(const char *tail, Position *pos, bool uciChess960) {
    scope(str_destroy) str_t token = str_init();
    tail = str_tok(tail, &token, " ");
    assert(tail);

    if (!strcmp(token.buf, "startpos")) {
        pos_set(pos, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", uciChess960);
        tail = str_tok(tail, &token, " ");
    } else if (!strcmp(token.buf, "fen")) {
        scope(str_destroy) str_t fen = str_init();

        while ((tail = str_tok(tail, &token, " ")) && strcmp(token.buf, "moves"))
            str_push(str_cat(&fen, token), ' ');

        if (!pos_set(pos, fen.buf, uciChess960))
            DIE("Illegal FEN '%s'\n", fen.buf);
    } else
        assert(false);

    if (!strcmp(token.buf, "moves")) {
        Position p[2];
        int idx = 0;
        p[0] = *pos;

        while ((tail = str_tok(tail, &token, " "))) {
            const move_t m = pos_lan_to_move(&p[idx], token.buf);
            pos_move(&p[1 - idx], &p[idx], m);
            idx = 1 - idx;
        }

        *pos = p[idx];
    }
}

static void random_pv(const Position *pos, uint64_t *seed, int len, str_t *pv) {
    str_clear(pv);
    Position p[2];
    p[0] = *pos;
    move_t *vecMoves = vec_init_reserve(64, move_t);
    scope(str_destroy) str_t lan = str_init();

    for (int ply = 0; ply < len; ply++) {
        // Generate and count legal moves
        vecMoves = gen_all_moves(&p[ply % 2], vecMoves);
        const uint64_t n = (uint64_t)vec_size(vecMoves);
        if (n == 0)
            break;

        // Choose a random one
        const move_t m = vecMoves[prng(seed) % n];
        pos_move_to_lan(&p[ply % 2], m, &lan);
        str_push(str_cat(pv, lan), ' ');
        pos_move(&p[(ply + 1) % 2], &p[ply % 2], m);
    }

    vec_destroy(vecMoves);
}

static void run_go(const Position *pos, const Go *go, uint64_t *seed) {
    scope(str_destroy) str_t pv = str_init();

    for (int depth = 1; depth <= go->depth; depth++) {
        random_pv(pos, seed, depth, &pv);
        uci_printf("info depth %d score cp %d pv %s\n", depth,
                   (int)((prng(seed) & 0xFFFF) - 0x8000), pv.buf);
    }

    scope(str_destroy) str_t token = str_init();
    str_tok(pv.buf, &token, " ");
    uci_printf("bestmove %s\n", token.buf);
}

int main(int argc, char **argv) {
    if (argc >= 2 && !strcmp(argv[1], "-version")) {
        puts("c-chess-cli/test " VERSION);
        return 0;
    }

    Position pos = {0};
    Go go = {0};
    bool uciChess960 = false;
    const uint64_t originalSeed = argc > 1 ? (uint64_t)atoll(argv[1]) : 0;
    uint64_t seed = originalSeed;

    scope(str_destroy) str_t line = str_init();

    while (str_getline(&line, stdin)) {
        const char *tail = NULL;

        if (!strcmp(line.buf, "uci")) {
            uci_puts("id name engine");
            uci_printf("option name UCI_Chess960 type check default %s\n",
                       uciChess960 ? "true" : "false");
            uci_puts("uciok");
        } else if (!strcmp(line.buf, "ucinewgame"))
            seed = originalSeed; // make results reproducible across ucinewgame
        else if (!strcmp(line.buf, "isready"))
            uci_puts("readyok");
        else if ((tail = str_prefix(line.buf, "setoption "))) {
            tail = str_prefix(tail, "name UCI_Chess960 value ");
            uciChess960 = tail && !strcmp(tail, "true");
        } else if ((tail = str_prefix(line.buf, "position "))) {
            // Hash the position string into seed. This allows c-chess-cli test suite to exercise
            // concurrency, while keeping PGN output identical.
            hash_blocks(line.buf, line.len, &seed);
            parse_position(tail, &pos, uciChess960);
        } else if ((tail = str_prefix(line.buf, "go "))) {
            tail = str_prefix(tail, "depth ");
            go.depth = tail ? atoi(tail) : 0;
            run_go(&pos, &go, &seed);
        } else if (!strcmp(line.buf, "quit"))
            break;
    }
}
