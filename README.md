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

### Example

```
./c-chess-cli -cmd demolito:../Engines/critter_1.6a -options Hash=8,Threads=2:Threads=1 \
    -games 8 -concurrency 4 -openings ../spsa/book.epd -repeat -movetime 0.15 -depth 10 \
    -resign 3,700 -draw 8,10 -pgnout out.pgn
```

### Options

Syntax: `-<option> <value>`.

List of `<option> <value>`:
- `concurrency C`: number of games played concurrently.
- `draw N,S`: draw when |score| <= `S` (in cp) for `N` consecutive moves, for both sides.
- `games N`: number of games to play.
- `openings FILE`: input file, in EPD format, where opening positions are read.
- `pgnout FILE`: output file, in PGN format, where games are written.
- `resign N,S`: resign when score <= -`S` (in cp) for `N` consecutive moves, for the losing side.
- `tc X/Y+Z`: set time control to `X` moves in `Y` sec (repeating) + `Z` sec increment per move. For
  example, `Y/Y` corresponds to a tournament time control, `Y+Z` corresponds to an increment time
  control, and just `Y` corresponds to a sudden death time control.

### Flags

Syntax: `-<flag>`.

List:
- `chess960`: use Chess960/FRC castling rules.
- `debug`: write all I/O communication with engines to file(s). This produces `c-chess-cli.id.log`,
where `id` is the thread id (range `0..concurrency-1`).
- `random`: start from a random opening in the EPD file. Proceed sequentially afterwards.
- `repeat`: repeat each opening twice, with each engine playing both sides.

### Engine options

Syntax:
- `-<option> <value1>` gives `<value1>` to both engines.
- `-<option> <value1>:<value2>` gives `<value1>` to the first engine, and `<value2>` to the second.
- `-<option> <value1>:` gives `<value1>` to the first engine, and nothing to the second.
- `-<option> :<value2>` can you guess? good.

List:
- cmd: command to run each engine.
- name: name override for each engine. By default the name is read from `id name` following the UCI
  protocol (and if that fails cmd value will be used as name).
- movetime: time limit per move in seconds.
- nodes: node limit per move.
- options: UCI options per engine. In this context, `<value1>` and `<value2>`, must be comma
  separated, like so: `-options Hash=2,Threads=1:Book=true,Hash=4`. Special characters, like space,
  should be escaped using the shell. For example `-options Time\ Buffer=50`, or `-options "Time Buffer=50"`,
  or whatever is the correct syntax for your shell.
