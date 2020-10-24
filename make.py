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
cflags = '-I./src -std=gnu11 -mpopcnt {}'.format('-DNDEBUG -Ofast -flto -s' if not args.debug else '-g -O1')
wflags = '-Wfatal-errors -Wall -Wextra -Wstrict-prototypes -Wsign-conversion -Wshadow -Wpadded'
lflags ='-lpthread -lm'
if args.static: lflags += ' -static'
if 'ANDROID_DATA' in os.environ: lflags += ' -latomic'

def run(cmd):
    print('> ' + cmd)
    return os.system(cmd)

def compile(program, output):
    sources = 'src/bitboard.c src/gen.c src/position.c src/str.c src/util.c src/vec.c'
    if program == 'main':
        sources += ' src/engine.c src/game.c src/main.c src/openings.c src/options.c src/sprt.c' \
            ' src/workers.c'
    elif program == 'engine':
        sources += ' test/engine.c'

    return run('{} {} {} {} -o {} {}'.format(args.compiler, cflags, wflags, sources, output, lflags))

if args.task == 'test':
    if compile('engine', './test/engine') == 0 and compile('main', './c-chess-cli') == 0:
        run('rm stdout1 stdout2 out.pgn training.csv c-chess-cli.1.log')
        run('./c-chess-cli -each cmd=./test/engine depth=6 option.Hash=4 ' \
            '-engine "name=engine\:1" option.Threads=2 -engine name=engine2 depth=5 ' \
            '-sample 0.5,y,training.csv -openings test/chess960.epd -repeat ' \
            '-resign 4,9000 -draw 2,10000 -games 1925 -log -pgnout out.pgn > stdout1')
        run('./c-chess-cli -each cmd=./test/engine depth=7 option.Hash=4 ' \
            '-engine "name=engine\:1" option.Threads=2 -engine name=engine2 ' \
            '-openings test/chess960.epd -games 100 > stdout2')
        print('\nFile signatures:')
        run('shasum stdout1 stdout2 out.pgn training.csv')
        run('grep -v ^deadline: c-chess-cli.1.log |shasum')
        print('\nOverall signature:')
        run('grep -v ^deadline: c-chess-cli.1.log | cat - stdout1 stdout2 out.pgn training.csv |shasum')

elif args.task == 'main':
    if args.output == '': args.output = './c-chess-cli'
    compile(args.task, args.output)

elif args.task == 'engine':
    if args.output == '': args.output = './test/engine'
    compile(args.task, args.output)
