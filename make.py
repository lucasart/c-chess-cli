#!/usr/bin/python3
import argparse, os, subprocess

p = argparse.ArgumentParser(description='c-chess-cli build script')
p.add_argument('-c', '--compiler', help='Compiler', choices=['cc', 'gcc', 'clang', 'musl-gcc',
    'musl-clang', 'x86_64-w64-mingw32-gcc'], default='cc')
p.add_argument('-o', '--output', help='Output file', default='')
p.add_argument('-d', '--debug', action='store_true', help='Debug compile')
p.add_argument('-s', '--static', action='store_true', help='Static compile')
p.add_argument('-t', '--task', help='Task to run', choices=['main', 'test', 'engine', 'clean', 'format'], default='main')
args = p.parse_args()

# Determine flags for: compilation, warning, and linking
version = subprocess.run(['git', 'show','-s','--format=%ci'], stdout=subprocess.PIPE).stdout.split()[0].decode('utf-8')
cflags = '-I./src -std=gnu11 -mpopcnt {} -DVERSION=\\"{}\\"'.format('-DNDEBUG -Os -ffast-math -flto -s' if not args.debug else '-g -O1', version)

wflags = '-Wfatal-errors -Wall -Wextra -Wstrict-prototypes -Wsign-conversion -Wshadow -Wmissing-prototypes'
if 'clang' in args.compiler:
    wflags += ' -Wcast-align -Wmissing-variable-declarations -Wshorten-64-to-32 -Wimplicit-int-conversion -Wcomma'

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

def clean():
    run('rm -f c-chess-cli c-chess-cli.1.log log out1.pgn out2.pgn stdout test/engine training.csv')

if args.task == 'clean':
    clean()

elif args.task == 'format':
    run('clang-format -i **/*.c **/*.h')

elif args.task == 'test':
    clean()

    if compile('engine', './test/engine') == 0 and compile('main', './c-chess-cli') == 0:
        print('\nRun tests:')
        run('./c-chess-cli -each cmd=./test/engine depth=6 option.Hash=4 '
            '-engine name=engine=1 option.Threads=2 -engine name=engine2 depth=5 '
            '-openings file=test/chess960.epd order=random srand=1 -resign count=4 score=13733 '
            '-draw number=40 count=3 score=11077 -games 965 -pgn out1.pgn 2 -concurrency 8 > /dev/null')

        run('./c-chess-cli -each "cmd=./test/engine 123" depth=3 '
            '-engine option.Hash=2 tc=10/0 -engine name=e2 tc=20/0 -engine name=e3 '
            '-sample freq=0.5 decay=0.05 resolve=y file=training.csv format=csv '
            '-openings file=test/chess960.epd -repeat '
            '-rounds 3 -games 30 -resign number=35 count=5 score=8192 -pgn out2.pgn 2 -log > stdout')
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
