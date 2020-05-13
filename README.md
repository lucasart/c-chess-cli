## c-chess-cli

c-chess-cli is a command line interface for UCI chess engines written in C.

## How to compile ?

Using the makefile:
```
make [CC=comp] [OUT=file] [debug=yes/no] [static=yes/no]
```
- comp can be: `gcc`, `clang`, `musl-gcc`, or `musl-clang`.
- file is the executable output file name (default `c-chess-cli`).
- debug=yes produces a debug compile (slower but with `assert()` and debug info).
- static=yes produces a statically linked executable.

## How to use ?

High level syntax is: `c-chess-cli -cmd engine1[:engine2] [-option [...]] ...`.

- cmd `engine1:[engine2]`: sets first engine to `engine1`, and (optionally) second engine to `engine2`. For example `-cmd ../Engines/demolito:./critter`.

### General options

Syntax: `-option [value]`, where some options expect a value (eg. pgnout expects a file), others
don't (eg. repeat is just a flag).

- concurrency c: Number of threads used to play games concurrently. Default is `c=1`.
- games n: Number of games per threads. The total number of games played is therefore `n`*`c`.
- openings file: Takes `file` in EPD format for opening positions. Default is to use the starting
position for all games.
- pgnout file: Output file where games are written, in PGN format.
- chess960: Use Chess960/FRC castling rules.
- random: Start from a random opening in the EPD file. Proceed sequentially afterwards.
- repeat: Repeat each opening twice, with each engine playing both sides.
- debug: Write all I/O communication with engines to file(s). This produces `c-chess-cli.id.log`,
where `id` is the thread id (range `0..concurrency-1`).

### Per engine options

Syntax: `-option value1[:value2]`, where value1 applies to the first engine, and value2 to
the second. If value 2 is omitted, value1 is applied to both.

- ucioptions: List of UCI options per engine (default none). For example `-ucioptions Hash=2,Threads=4`
giving both engines the same option list. Or `-ucioptions Hash=2:Threads=4,Hash=8` giving different
option list per engine.
- nodes: Node limit per move (default none).
- depth: Depth limit per move (default none).
- movetime: Time limit per move in milliseconds (default none).
