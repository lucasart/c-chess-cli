#!/usr/bin/python
import argparse, os

p = argparse.ArgumentParser(description='c-chess-cli build script')
p.add_argument('-c', '--compiler', help='Compiler', choices=['cc', 'gcc', 'clang', 'musl-gcc',
    'musl-clang'], default='cc')
p.add_argument('-o', '--output', help='Output file', default='')
p.add_argument('-d', '--debug', action='store_true', help='Debug compile')
p.add_argument('-s', '--static', action='store_true', help='Static compile')
p.add_argument('-p', '--task', help='Task to run', choices=['main', 'test', 'engine'], default='main')
args = p.parse_args()

# Determine flags for: compilation, warning, and linking
cflags = '-I./src -std=gnu11 -mpopcnt {}'.format('-DNDEBUG -Os -ffast-math -flto -s' if not args.debug else '-g -O1')
wflags = '-Wfatal-errors -Wall -Wextra -Wstrict-prototypes -Wsign-conversion -Wshadow -Wno-unused-parameter'
lflags ='-lpthread -lm'
if args.static: lflags += ' -static'

def run(cmd):
    print('% ' + cmd)
    return os.system(cmd)

def compile(program, output):
    sources = 'src/bitboard.c src/gen.c src/position.c src/str.c src/util.c src/vec.c'
    if program == 'main':
        sources += ' src/engine.c src/game.c src/jobs.c src/main.c src/openings.c src/options.c' \
            ' src/seqwriter.c src/sprt.c src/workers.c'
    elif program == 'engine':
        sources += ' test/engine.c'

    return run('{} {} {} {} -o {} {}'.format(args.compiler, cflags, wflags, sources, output, lflags))

if args.task == 'test':
    if compile('engine', './test/engine') == 0 and compile('main', './c-chess-cli') == 0:
        run('rm stdout* out*.pgn training.csv c-chess-cli.*.log log*')

        print('\nRun tests:')
        run('./c-chess-cli -each cmd=./test/engine depth=6 option.Hash=4 ' \
            '-engine name=engine=1 option.Threads=2 -engine name=engine2 depth=5 ' \
            '-openings file=test/chess960.epd order=random srand=1 -repeat -resign 5 900000000 ' \
            '-draw 3 600000000 -games 1930 -pgn out1.pgn 2 -concurrency 8 > /dev/null')

        run('./c-chess-cli -each "cmd=./test/engine 123" depth=3 ' \
            '-engine option.Hash=2 tc=10/0 -engine name=e2 tc=20/0 -engine name=e3 ' \
            '-sample 0.5,y,training.csv -openings file=test/chess960.epd -rounds 3 ' \
            '-games 50 -pgn out2.pgn 2 -log > stdout')
        run('grep -v ^deadline c-chess-cli.1.log > log')

        print('\nFile signatures:')
        run('sha1sum stdout out1.pgn out2.pgn log training.csv')
        print('\nOverall signature:')
        run('cat stdout out1.pgn out2.pgn log training.csv |sha1sum')

elif args.task == 'main':
    if args.output == '': args.output = './c-chess-cli'
    compile(args.task, args.output)

elif args.task == 'engine':
    if args.output == '': args.output = './test/engine'
    compile(args.task, args.output)
