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
    -games 1920 -concurrency 8 -openings test/chess960.epd -random -repeat \
    -resign 3,700 -draw 8,10 -pgn out.pgn
```

### Options

 * `engine OPTIONS`: Add an engine defined by `OPTIONS` to the tournament.
 * `each OPTIONS`: Apply `OPTIONS` to each engine in the tournament.
 * `concurrency N`: Set the maximum number of concurrent games to N (default value 1).
 * `draw COUNT,SCORE`: Adjudicate the game as a draw, if the score of both engines is within `SCORE` centipawns from zero, for at least `COUNT` consecutive moves.
 * `resign COUNT,SCORE`: Adjudicate the game as a loss, if an engine's score is at least `SCORE` centipawns below zero, for at least `COUNT` consecutive moves.
 * `games N`: Play N games per encounter (default value 1). This value should be set to an even number in tournaments with more than two players to make sure that each player plays an equal number of games with white and black pieces.
 * `rounds N`: Multiply the number of rounds to play by `N` (default value 1).
 * `gauntlet`: Play a gauntlet tournament (first engine against the others). The default is to play a round-robin.
 * `sprt elo0,elo1[,alpha,beta]`: Performs a Sequential Probability Ratio Test for `H1: elo=elo1` vs `H0: elo=elo0`, where `alpha` is the type I error probability (false positive), and `beta` is type II error probability (false negative). Note that `alpha` and `beta` are optional, and their default value is 0.05. This can only be used in matches between two players.
 * `log`: Write all I/O communication with engines to file(s). This produces `c-chess-cli.id.log`, where `id` is the thread id (range `1..concurrency`). Note that all communications (including error messages) starting with `[id]` mean within the context of thread number `id`, which tells you which log file to inspect (id = 0 is the main thread, which does not product a log file, but simply writes to stdout).
 * `openings FILE`: Read opening positions from `FILE`, in EPD format. Note that Chess960 is auto-detected, at position level (not at file level), and `FILE` can mix Chess and Chess960 positions. Both X-FEN (KQkq) and S-FEN (HAha) are supported for Chess960.
 * `random`: Shuffle the opening set (play shuffled set sequentially, no repetitions).
 * `pgn FILE`: Save games to FILE, in PGN format.
 * `repeat`: Repeat each opening twice, with each engine playing both sides.
 * `sample freq[,resolvePv[,file]]`. See below.

### Engine options

 * `cmd=COMMAND`: Set the command to run the engine.
   * The current working directory will be set automatically, if a `/` is contained in `COMMAND`. For example, `cmd=../Engines/critter_1.6a`, will run `./critter_1.6a` from `../Engines`. If no `/` is found, the command is executed as is. Without `/`, for example `cmd=demolito` will run `demolito`, which only works if `demolito` is in `PATH`.
   * Arguments can be provided as part of the command. For example `"cmd=../fooEngine -foo=1"`. Note that the `""` are needed here, for the command line interpreter to parse the whole string as a single token.
 * `name=NAME`: Set the engine's name. If omitted, the name is take from the `id name` value sent by the engine.
 * `tc=TIMECONTROL`: Set the time control to `TIMECONTROL`. The format is `moves/time+increment`, where `moves` is the number of moves per tc, `time` is time per tc (in seconds), and `increment` is time increment per move (in seconds).
 * `st=N`: time limit per move, in seconds (can be fractional like `st=0.123`).
 * `depth=N`: depth limit per move.
 * `nodes=N`: node limit per move.
 * `option.OPTION=VALUE`: Set custom option OPTION to value VALUE.

### Sampling

The purpose of this feature is to the generate training data, which can be used to fit the parameters of a
chess engine evaluation, otherwise known as supervised learning.

Using `-sample freq[,resolvePv[,file]]` will generate a csv file of samples, in this format:
```
fen,score,result
```
where score is the search result (in cp), and result is the result of the game from the side to
move's perspective (0=loss, 1=draw, 2=win).

Using `resolvePv=y` does two things:
 * First, it resolves the PV, which means that it plays the PV and reocords the position at the end
  (leaf node), instea of the current position (root node).
 * Second, it guarantees that the recorded fen is not in check (by recording the last PV position
  that is not in check, if that is possible, else discarding the sample).
