## c-chess-cli

c-chess-cli is a command line interface for UCI chess engines written in C, for POSIX systems.

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

### Example

```
./c-chess-cli -cmd demolito:../Engines/critter_1.6a -options Hash=8,Threads=2:Threads=1 \
    -games 8 -concurrency 4 -openings ../spsa/book.epd -repeat -movetime 100 -depth 10 \
    -resign 3,700 -draw 8,10 -pgnout out.pgn
```

### General options

Syntax: `-option value`, where some options expect a value, others don't (just flags).

- chess960: Use Chess960/FRC castling rules.
- concurrency c: Number of threads used to play games concurrently.
- debug: Write all I/O communication with engines to file(s). This produces `c-chess-cli.id.log`,
where `id` is the thread id (range `0..concurrency-1`).
- draw n,s: Draw when |score| <= s (in cp) for n consecutive moves, for both sides.
- games n: Number of games
- openings file: Takes `file` in EPD format for opening positions.
- pgnout file: Output file where games are written, in PGN format.
- random: Start from a random opening in the EPD file. Proceed sequentially afterwards.
- resign n,s: Resign when score <= -s (in cp) for n consecutive moves, for the losing side.
- repeat: Repeat each opening twice, with each engine playing both sides.
- tc x/y+z: set time control to x moves in y sec (repeating) + z sec increment per move. x/y
  corresponds to a standard tournament time control, y+z corresponds to a standard increment time
  control, and y corresponds to a sudden death time control.

### Engine options

Syntax:
- `-option value1`: gives value1 to both engines.
- `-option value1:value2`: gives value1 to the first engine, and value2 to the second engine.
- `-option value1:`: gives value1 to the first engine, and nothing to the second.
- `-option :value2`: can you guess? good.

- cmd `engine1:[engine2]`: sets first engine to `engine1`, and second engine to `engine2`.
- movetime: Time limit per move in milliseconds (default none).
- nodes: Node limit per move (default none).
- options: List of UCI options per engine (default none).
