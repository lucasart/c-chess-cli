## c-chess-cli

c-chess-cli is a command line interface for UCI chess engines written in C. Free from [dependency hell](https://en.wikipedia.org/wiki/Dependency_hell), it only uses the C standard library, and POSIX. Primarily developped on Linux, it should work on all POSIX operating systems, including MacOS and Android. Windows support was added recently, and is still experimental at this stage.

## How to compile ?

Run `make.py` script, without any parameters.

See `make.py --help` for more options.

## How to use ?

```
c-chess-cli [-each [eng_options]] -engine [eng_options] -engine [eng_options] ... [options]
c-chess-cli -version
```

### Example

```
./c-chess-cli -each tc=4+0.04 option.Hash=8 option.Threads=1 \
    -engine cmd=demolito "option.Time Buffer=80" nodes=200000 \
    -engine cmd=../Engines/critter_1.6a depth=11 \
    -games 1920 -concurrency 8 -openings file=test/chess960.epd order=random -repeat \
    -sprt elo0=1 elo1=5 -resign count=3 score=700 -draw number=40 count=8 score=10 -pgn out.pgn 2
```

### Options

 * `engine OPTIONS`: Add an engine defined by `OPTIONS` to the tournament.
 * `each OPTIONS`: Apply `OPTIONS` to each engine in the tournament.
 * `concurrency N`: Set the maximum number of concurrent games to N (default value 1).
 * `draw [number=N] count=C score=S`: Adjudicate the game as a draw, if the score of both engines is within `S` centipawns from zero, for at least `C` consecutive moves, and at least `N` moves have been played (default value `N=0`).
 * `resign [number=N] count=C score=S`: Adjudicate the game as a loss, if an engine's score is at least `S` centipawns below zero, for at least `C` consecutive moves, and at least `N` moves have been played (default value `N=0`).
 * `games N`: Play N games per encounter (default value 1). This value should be set to an even number in tournaments with more than two players to make sure that each player plays an equal number of games with white and black pieces.
 * `rounds N`: Multiply the number of rounds to play by `N` (default value 1). This only makes sense to use for tournaments with more than 2 engines.
 * `gauntlet`: Play a gauntlet tournament (first engine against the others). The default is to play a round-robin (plays all pairs).
   * with `n=2` engines, both gauntlet and round-robin just play the number of `-games` specified.
   * gauntlet for `n>2`: `G(e1, ..., en) = G(e1, e2) + G(e1, e3) + ... + G(e1, en)`. There are `n-1` pairs.
   * round-robin for `n>2`: `RR(e1, ..., en) = G(e1, ..., en) + RR(e2, ..., en)`. There are `n(n-1)/2` pairs.
   * using `-rounds` repeats the tournament `-rounds` times. The number of games played for each pair is therefore `-games * -rounds`.
 * `sprt [elo0=E0] [elo1=E1] [alpha=A] [beta=B]`: Performs a Sequential Probability Ratio Test for `H1: elo=E1` vs `H0: elo=E0`, where `alpha` is the type I error probability (false positive), and `beta` is type II error probability (false negative). Default values are `elo0=0`, `elo1=4`, and `alpha=beta=0.05`. This can only be used in matches between two players.
 * `log`: Write all I/O communication with engines to file(s). This produces `c-chess-cli.id.log`, where `id` is the thread id (range `1..concurrency`). Note that all communications (including error messages) starting with `[id]` mean within the context of thread number `id`, which tells you which log file to inspect (id = 0 is the main thread, which does not product a log file, but simply writes to stdout).
 * `openings file=FILE [order=ORDER] [srand=N]`:
   * Read opening positions from `FILE`, in EPD format. Note that Chess960 is auto-detected, at position level (not at file level), and `FILE` can mix Chess and Chess960 positions. Both X-FEN (KQkq) and S-FEN (HAha) are supported for Chess960.
   * `order` can be `random` or `sequential` (default value).
   * `srand` sets the seed of the random number generator to `N`. The default value `N=0` will set the seed automatically to an unpredictable number. Any non-zero number will generate a unique, reproducible random sequence.
 * `pgn FILE [VERBOSITY]`: Save games to `FILE`, in PGN format. `VERBOSITY` is optional
   * `0` produces a PGN with headers and results only, which can be used with rating tools like BayesElo or Ordo.
   * `1` adds the moves to the PGN.
   * `2` adds comments of the form `{score/depth}`.
   * `3` (default value) adds time usage to the comments `{score/depth time}`.
 * `repeat`: Repeat each opening twice, with each engine playing both sides.
 * `sample`. See below.

### Engine options

 * `cmd=COMMAND`: Set the command to run the engine.
   * The current working directory will be set automatically, if a `/` is contained in `COMMAND`. For example, `cmd=../Engines/critter_1.6a`, will run `./critter_1.6a` from `../Engines`. If no `/` is found, the command is executed as is. Without `/`, for example `cmd=demolito` will run `demolito`, which only works if `demolito` is in `PATH`.
   * Arguments can be provided as part of the command. For example `"cmd=../fooEngine -foo=1"`. Note that the `""` are needed here, for the command line interpreter to parse the whole string as a single token.
 * `name=NAME`: Set the engine's name. If omitted, the name is take from the `id name` value sent by the engine.
 * `tc=TIMECONTROL`: Set the time control to `TIMECONTROL`. The format is `moves/time+increment`, where `moves` is the number of moves per tc, `time` is time per tc (in seconds), and `increment` is time increment per move (in seconds). Note that `moves` and `increment` are optional, so you can use `moves/time` or `time+increment`.
 * `movetime=N`: time limit per move, in seconds (can be fractional like `movetime=0.123`).
 * `depth=N`: depth limit per move.
 * `nodes=N`: node limit per move.
 * `timeout=N`: tolerance (in seconds) for c-chess-cli to determine when an engine hangs (which is an unrecovrable error at this point). Default value is `N=4`.
 * `option.O=V`: Set UCI option `O` to value `V`.

### Sampling (advanced)

The purpose of this feature is to the generate training data, which can be used to fit the parameters of a chess engine evaluation, otherwise known as supervised learning.

Syntax is `-sample [freq=%f] [decay=%f] [resolve=y|n] [file=%s] [format=csv|bin]`. Example `-sample freq=0.25 resolve=y file=out.csv format=csv`.
 * `freq` is the sampling frequency (floating point number between `0` and `1`). Defaults to `1` if omitted.
 * `decay` is the sample decay based on the rule50 counter. Sampling probability is `freq * exp(-decay * rule50)`, where `rule50` ranges from `0` to `99` is the ply counter for the 50-move draw rule. Defaults to `0` if omitted.
 * `resolve` is `y` for tactical resolution, and `n` (default) otherwise. Tactical resolution is done as follows:
   * Solve tactical sequences: by playing all tactical moves at the start of the PV, to record the first quiet position.
   * Excludes checks: by recording the last PV position that is not in check (if all PV positions are in check, the sample is discarded).
   * Exclude mates: by discarding samples where the engine returns a mate score.
 * `file` is the name of the file where samples are written. Defaults to `sample.csv|bin` if omitted.
 * `format` is the format in which the file is written. Defaults to `csv`, which is human readable: `FEN,Eval,Result`. `Eval` is the score in cp, as returned by the engine, except for mate scores encoded as `INT16_MAX - dtm` (mating) or `INT16_MIN + dtm` (mated). Values for `Result` are `0=loss`, `1=draw`, `2=win`. Binary format `bin` uses variable length encoding shown below.

Entries in binary format (28 bytes max, average 24 or less):
```
uint64_t occ;    // occupied squares (bitboard)
uint8_t turn:1, rule50:7;  // turn: 0=WHITE, 1=BLACK; rule50: half-move clock for 50-move rule
uint8_t packedPieces[(count(occ) + 1) / 2];  // 4 bits per piece, max 16 bytes
int16_t score;   // score in cp; mating scores INT16_MAX - dtm; mated scores INT16_MIN + dtm
uint8_t result;  // 0=loss, 1=draw, 2=win
```
Encoding for `packedPieces[]` elements is:
```
0=KNIGHT, 1=BISHOP, 2=ROOK, 3=QUEEN, 4=KING, 5=PAWN,
6=ROOK with castling ability,
7=PAWN which is capturable en-passant
```
