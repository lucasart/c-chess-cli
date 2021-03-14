## c-chess-cli

c-chess-cli is a command line interface for UCI chess engines written in C. Free from [dependency hell](https://en.wikipedia.org/wiki/Dependency_hell), it only uses the C standard library, and POSIX. Primarily developped on Linux, it should work on all POSIX operating systems, including MacOS and Android. Windows is not supported.

## How to compile ?

Run `make.py` script, without any parameters.

See `make.py --help` for more options.

## How to use ?

```
c-chess-cli [-each [eng_options]] -engine [eng_options] -engine [eng_options] ... [options]
```

### Example

```
./c-chess-cli -each tc=4+0.04 option.Hash=8 option.Threads=1 \
    -engine cmd=demolito "option.Time Buffer=80" nodes=200000 \
    -engine cmd=../Engines/critter_1.6a depth=11 \
    -games 1920 -concurrency 8 -openings file=test/chess960.epd order=random -repeat \
    -sprt elo0=1 elo1=5 -resign 3 700 -draw 8 10 -pgn out.pgn 2
```

### Options

 * `engine OPTIONS`: Add an engine defined by `OPTIONS` to the tournament.
 * `each OPTIONS`: Apply `OPTIONS` to each engine in the tournament.
 * `concurrency N`: Set the maximum number of concurrent games to N (default value 1).
 * `draw COUNT SCORE`: Adjudicate the game as a draw, if the score of both engines is within `SCORE` centipawns from zero, for at least `COUNT` consecutive moves.
 * `resign COUNT SCORE`: Adjudicate the game as a loss, if an engine's score is at least `SCORE` centipawns below zero, for at least `COUNT` consecutive moves.
 * `games N`: Play N games per encounter (default value 1). This value should be set to an even number in tournaments with more than two players to make sure that each player plays an equal number of games with white and black pieces.
 * `rounds N`: Multiply the number of rounds to play by `N` (default value 1). This only makes sense to use for tournaments with more than 2 engines.
 * `gauntlet`: Play a gauntlet tournament (first engine against the others). The default is to play a round-robin (plays all pairs).
   * with `n=2` engines, both gauntlet and round-robin just play the number of `-games` specified.
   * gauntlet for `n>2`: `G(e1, ..., en) = G(e1, e2) + G(e2, ..., en)`. There are `n-1` pairs.
   * round-robin for `n>2`: `RR(e1, ..., en) = G(e1, ..., en) + RR(e2, ..., en)`. There are `n(n-1)/2` pairs.
   * using `-rounds` repeats the tournament `-rounds` times. The number of games played for each pair is therefore `-games * -rounds`.
 * `sprt [elo0=E0] elo1=E1 [alpha=A] [beta=B]`: Performs a Sequential Probability Ratio Test for `H1: elo=E1` vs `H0: elo=E0`, where `alpha` is the type I error probability (false positive), and `beta` is type II error probability (false negative). Default values are `elo0=0`, and `alpha=beta=0.05`. This can only be used in matches between two players.
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
 * `sample freq[,resolvePv[,file]]`. See below.

### Engine options

 * `cmd=COMMAND`: Set the command to run the engine.
   * The current working directory will be set automatically, if a `/` is contained in `COMMAND`. For example, `cmd=../Engines/critter_1.6a`, will run `./critter_1.6a` from `../Engines`. If no `/` is found, the command is executed as is. Without `/`, for example `cmd=demolito` will run `demolito`, which only works if `demolito` is in `PATH`.
   * Arguments can be provided as part of the command. For example `"cmd=../fooEngine -foo=1"`. Note that the `""` are needed here, for the command line interpreter to parse the whole string as a single token.
 * `name=NAME`: Set the engine's name. If omitted, the name is take from the `id name` value sent by the engine.
 * `tc=TIMECONTROL`: Set the time control to `TIMECONTROL`. The format is `moves/time+increment`, where `moves` is the number of moves per tc, `time` is time per tc (in seconds), and `increment` is time increment per move (in seconds).
 * `movetime=N`: time limit per move, in seconds (can be fractional like `movetime=0.123`).
 * `depth=N`: depth limit per move.
 * `nodes=N`: node limit per move.
 * `option.OPTION=VALUE`: Set custom option OPTION to value VALUE.

### Sampling

The purpose of this feature is to the generate training data, which can be used to fit the parameters of a
chess engine evaluation, otherwise known as supervised learning.

Syntax is `-sample freq[,resolve[,binary,[file]]]`. Example `-sample 0.25,y,n,out.csv`.
 * `freq` is the sampling frequency (floating point number between `0` and `1`).
 * `resolve` is `y` for PV resolution, and `n` (default) otherwise. PV resolution does the following:
   * Plays the PV and reocords the position at the end (leaf node), instead of the current position (root node).
   * Guarantees that the recorded positions are not in check (by recording the last PV position that is not in check, if all PV positions are in check, the sample is discarded).
 * `binary` is `y` for binary format, and `n` (default) for text format.
   * text format is a human readable CSV whose lines look like `fen,score,result`, where `score` is in centipawns, and result is `-1=lose`, `0=draw`, `1=win` (all from the side to move's pov).
   * binary follows the NNUE training data format (see code for documentation).
 * `file` is the name of the file where samples are written. Defaults to `sample.csv` if omitted.
