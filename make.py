#!/usr/bin/python
import argparse, os

p = argparse.ArgumentParser(description='c-chess-cli build script')
p.add_argument('-c', '--compiler', help='Compiler', choices=['gcc', 'clang', 'musl-gcc',
    'musl-clang'], default='gcc')
p.add_argument('-o', '--output', help='Output', default='./c-chess-cli')
p.add_argument('-d', '--debug', action='store_true')
p.add_argument('-s', '--static', action='store_true')
p.add_argument('-t', '--test', action='store_true', help="compile and run tests")
args = p.parse_args()

sources = ['bitboard.c', 'util.c']
if args.test:
    sources += ['test.c']
else:
    sources += ['main.c', 'engine.c', 'game.c', 'gen.c', 'openings.c', 'options.c', 'position.c', 'sprt.c', 'str.c', 'workers.c']

cflags = '-std=gnu11 {} -mpopcnt -Ofast -flto'.format('-DNDEBUG -s' if not args.debug else '-g')
wflags = '-Wfatal-errors -Wall -Wextra -Wstrict-prototypes -Wsign-conversion'
lflags ='-lpthread -lm {}'.format('-static' if args.static else '')
cmd = '{} {} {} {} -o {} {}'.format(args.compiler, cflags, wflags, (" ").join(sources),args.output, lflags)
print(cmd)
os.system(cmd)

if args.test:
    print("running tests...")
    os.system(args.output)
