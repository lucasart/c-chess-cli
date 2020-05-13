## c-chess-cli

c-chess-cli is a command line interface for UCI chess engines written in C.

## How to compile ?

Using the makefile:
```
make [CC=comp] [OUT=file] [debug=yes/no] [static=yes/no]
```
- comp: `gcc`, `clang`, `musl-gcc`, or `musl-clang`.
- file: executable output file name (default `c-chess-cli`).
- debug=yes: debug compile (slower but with `assert()` and debug info).
- static=yes: statically linked executable.

## How to use ?

`c-chess-cli [-option [value1[:value2]]] ...`

### General options

Syntax: `-option [value]`, where some options expect a value, others don't (just flags).

- concurrency c: Number of threads used to play games concurrently.
- games n: Number of games
- openings file: Takes `file` in EPD format for opening positions.
- pgnout file: Output file where games are written, in PGN format.
- chess960: Use Chess960/FRC castling rules.
- random: Start from a random opening in the EPD file. Proceed sequentially afterwards.
- repeat: Repeat each opening twice, with each engine playing both sides.
- debug: Write all I/O communication with engines to file(s). This produces `c-chess-cli.id.log`,
where `id` is the thread id (range `0..concurrency-1`).

### Per engine options

Syntax: `-option value1[:value2]`, where value1 applies to the first engine, and value2 to
the second. If value 2 is omitted, value1 is applied to both.

- cmd `engine1:[engine2]`: sets first engine to `engine1`, and (optionally) second engine to `engine2`.
- ucioptions: List of UCI options per engine (default none). For example `-ucioptions Hash=2,Threads=4`
giving both engines the same option list. Or `-ucioptions Hash=2:Threads=4,Hash=8` giving different
option list per engine.
- nodes: Node limit per move (default none).
- depth: Depth limit per move (default none).
- movetime: Time limit per move in milliseconds (default none).
