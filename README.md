## c-chess-cli

c-chess-cli is a command line interface for UCI chess engines written in C. Free from [dependency hell](https://en.wikipedia.org/wiki/Dependency_hell), it only uses the C standard library, and POSIX. Primarily developped on Linux, it should work on all POSIX operating systems, including MacOS and Android. Windows is not supported, because it is not POSIX compliant.

## How to compile ?

Run `make.py` script, without any parameters.

See `make.py --help` for more options.

## How to use ?

### Example

```
./c-chess-cli -each tc=4+0.04 option.Hash=8 option.Threads=1 \
    -engine cmd=demolito "option.Time Buffer=80" nodes=200000 \
    -engine cmd=../Engines/critter_1.6a depth=11 \
    -games 1920 -concurrency 8 -openings test/chess960.epd -random -repeat \
    -resign 3,700 -draw 8,10 -pgnout out.pgn
```

### Options

Syntax: `-option value`.

List of `option value`:
 * `concurrency c`: number of games played concurrently.
 * `draw n,s`: draw when |score| <= `s` (in cp) for `n` consecutive moves, for both sides.
 * `games n`: number of games to play.
 * `openings file`: input file, in EPD format, where opening positions are read. Note that
  chess960 is auto-detected by analysing each FEN, so there is no command line parameter
  for it.
 * `pgnout file`: output file, in PGN format, where games are written.
 * `resign n,s`: resign when score <= -`s` (in cp) for `n` consecutive moves, for the losing side.
 * `sample freq[,resolvePv[,file]]`: collects sample of position and search score. `freq` is the frequency (between
  0 and 1), `resolvePv` is either `y` or `n`, and `file` is the output file (if omitted, defaults to `sample.csv`).
  More details in Sample section below.
 * `sprt elo0,elo1,alpha,beta`: performs a [Sequential Probability Ratio Test](https://en.wikipedia.org/wiki/Sequential_probability_ratio_test)
  for `H1: elo=elo1` vs `H0: elo=elo0`, where `alpha` is the type I error probability (false positive),
  and `beta` is type II error probability (false negative). It uses the GSPRT approximation of the LLR
  derived by Michel Van den Bergh [here](http://hardy.uhasselt.be/Toga/GSPRT_approximation.pdf).
  Note that `alpha` and `beta` are optional, and their default value is 0.05.

### Flags

Syntax: `-flag`.

List:
 * `log`: write all I/O communication with engines to file(s). This produces `c-chess-cli.id.log`,
where `id` is the thread id (range `1..concurrency`). Note that all communications (including
error messages) starting with `[id]` mean within the context of thread number `id`, which tells you
which log file to inspect (id = 0 is the main thread).
 * `random`: shuffle the opening set (play shuffled set sequentially, no repetitions).
 * `repeat`: repeat each opening twice, with each engine playing both sides.

### Engine options

Syntax: `engine key1=value1 ... keyN=valueN`

Keys:
 * `cmd`: command to run each engine. The current working directory will be set automatically, if a
  `/` is contained in the value string(s). For example, `cmd=../Engines/critter_1.6a`, will run
  `./critter_1.6a` from `../Engines`. If no `/` is found, the command is executed as is. Without `/`,
  for example `cmd=demolito` will run `demolito`, which only works if `demolito` is in `PATH`.
 * `name`: name override for each engine. By default the name is read from `id name` following the UCI
  protocol (and if that fails cmd value will be used as name).
 * `option.name`: UCI option for engine. Several can be specified, for example `option.Hash=2 option.Threads=1`.
  Special characters, like space, should be escaped using the appropriate shell syntax. For example
  `option.Time\ Buffer=50`, or `"option.Time Buffer=50"`.
 * `depth`: depth limit per move.
 * `movetime`: time limit per move, in seconds (can be fractional like `movetime=0.123`).
 * `nodes`: node limit per move.
 * `tc`: expects value of the form `[mtg/]time[+inc]`. For example:
   * `40/10` corresponds to a tournament time control with 10s for every 40 moves
   * `10+0.1` corresponds to an increment time control with 10s for the game and 100ms increment per move
   * `10` corresponds to a sudden death time control with 10s for the game
   * `40/10+0.1` is possible, but does not correspond to any standard chess clock system.

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
