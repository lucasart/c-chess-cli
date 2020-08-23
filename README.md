## c-chess-cli

c-chess-cli is a command line interface for UCI chess engines written in C. Free from [Dependency hell](https://en.wikipedia.org/wiki/Dependency_hell), it only uses the C standard library, and POSIX. For Windows, you must run it in a POSIX enviroment, such as an [msys2](https://www.msys2.org/).

## How to compile ?

Run `make.py` script, without any parameters.

See `make.py --help` for more options.

## How to use ?

### Example

```
./c-chess-cli -cmd demolito:../Engines/critter_1.6a -options Hash=8,Threads=2:Threads=1 \
    -games 8 -concurrency 4 -openings test/chess960.epd -repeat -tc 2+0.02 -depth 10 \
    -resign 3,700 -draw 8,10 -pgnout out.pgn
```

### Options

Syntax: `-option value`.

List of `option value`:
- `concurrency c`: number of games played concurrently.
- `draw n,s`: draw when |score| <= `s` (in cp) for `n` consecutive moves, for both sides.
- `games n`: number of games to play.
- `openings file`: input file, in EPD format, where opening positions are read. Note that
  chess960 is auto-detected by analysing each FEN, so there is no command line parameter
  for it.
- `pgnout file`: output file, in PGN format, where games are written.
- `resign n,s`: resign when score <= -`s` (in cp) for `n` consecutive moves, for the losing side.
- `sample freq[,file]`: collects sample of position and search score. `freq` is the frequency (between
  0 and 1), and `file` is the output file (if omitted, defaults to `sample.bin`). More details in
  Sample section below.
- `sprt elo0,elo1,alpha,beta`: performs a [Sequential Probability Ratio Test](https://en.wikipedia.org/wiki/Sequential_probability_ratio_test)
  for `H1: elo=elo1` vs `H0: elo=elo0`, where `alpha` is the type I error probability (false positive),
  and `beta` is type II error probability (false negative). It uses the GSPRT approximation of the LLR
  derived by Michel Van den Bergh [here](http://hardy.uhasselt.be/Toga/GSPRT_approximation.pdf).
  Note that `alpha` and `beta` are optional, and their default value is 0.05.
- `tc X/Y+Z`: set time control to `X` moves in `Y` sec (repeating) + `Z` sec increment per move. For
  example, `X/Y` corresponds to a tournament time control, `Y+Z` corresponds to an increment time
  control, and just `Y` corresponds to a sudden death time control.

### Flags

Syntax: `-flag`.

List:
- `debug`: write all I/O communication with engines to file(s). This produces `c-chess-cli.id.log`,
where `id` is the thread id (range `1..concurrency`). Note that all communications (including
error messages) starting with `[id]` mean within the context of thread number `id`, which tells you
which log file to inspect (id = 0 is the main thread).
- `random`: start from a random opening in the EPD file. Proceed sequentially afterwards.
- `repeat`: repeat each opening twice, with each engine playing both sides.

### Engine options

Syntax:
- `-option value1` gives `value1` to both engines.
- `-option value1:value2` gives `value1` to the first engine, and `value2` to the second.
- `-option value1:` gives `value1` to the first engine, and nothing to the second.
- `-option :value2` can you guess? good.

List:
- `cmd`: command to run each engine. The current working directory will be set automatically, if a
  `/` is contained in the value string(s). For example, `../Engines/critter_1.6a`, will run
  `./critter_1.6a` from `../Engines`. If no `/` is found, the command is executed as is. For example
  `demolito` will simply run `demolito`, which only works if `demolito` is in `PATH`.
- `name`: name override for each engine. By default the name is read from `id name` following the UCI
  protocol (and if that fails cmd value will be used as name).
- `movetime`: time limit per move in seconds.
- `nodes`: node limit per move.
- `options`: UCI options per engine. In this context, `value1` and `value2`, must be comma
  separated, like so: `-options Hash=2,Threads=1:Book=true,Hash=4`. Special characters, like space,
  should be escaped using the appropriate shell syntax. For example `-options Time\ Buffer=50`, or `-options "Time Buffer=50"`.

### Sampling

This is only of interest for chess engine programmers. The intent of this feature is to the generate
training data, which can be used to teach a neural network to evaluate chess positions.

Using `-sample freq[,file]` will generate a binary file of samples, in this format:
```
struct {
    uint64_t byColor[NB_COLOR];  // bit #i is set if piece of color on square #i
    uint64_t byPiece[NB_PIECE];  // bit #i is set if piece of type on square #i
    int16_t score;  // score returned by the engine (in cp)
    int8_t result;  // game result from turn's pov: -1 (loss), 0 (draw), +1 (win)
    uint8_t turn;  // turn of play
    uint8_t epSquare;  // en-passant square (NB_SQUARE if none)
    uint8_t castleRooks[NB_COLOR];  // rooks available for castling; a file bitmask by color
    uint8_t rule50; // ply counter for 50-move rule, from 0 to 99 (100 would be draw or mated)
};
```
Where color, piece, file, and square indexing are as follows:
```
enum {WHITE, BLACK, NB_COLOR};
enum {KNIGHT, BISHOP, ROOK, QUEEN, KING, PAWN, NB_PIECE};
enum {FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H, NB_FILE};
enum {
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
    NB_SQUARE
};
```
