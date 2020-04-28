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
#include "os.h"
#include <string.h>

int main(int argc, char **argv)
{
    if (argc > 1) {
        Process p;
        process_create(&p, argv[1]);

        char buf[0x100];
        process_writeln(&p, "uci\n");

        while (true) {
            process_readln(&p, buf, sizeof(buf));
            printf("%s", buf);

            if (!strcmp(buf, "uciok\n"))
                break;
        }

        process_writeln(&p, "position startpos\n");
        process_writeln(&p, "ucinewgame\n");
        process_writeln(&p, "go depth 5\n");

        while (true) {
            process_readln(&p, buf, sizeof(buf));
            printf("%s", buf);

            if (strstr(buf, "bestmove"))
                break;
        }

        process_writeln(&p, "quit\n");
        process_terminate(&p);
    }
}
