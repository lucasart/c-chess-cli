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

# Select source files, depending on which program is being compiled
sources = 'bitboard.c gen.c position.c str.c util.c'
if args.test:
    sources += ' test.c'
else:
    sources += ' engine.c game.c main.c openings.c options.c sprt.c workers.c'

# Determine flags for: compilation, warning, and linking
cflags = '-std=gnu11 {} -mpopcnt -Ofast -flto'.format('-DNDEBUG -s' if not args.debug else '-g')
wflags = '-Wfatal-errors -Wall -Wextra -Wstrict-prototypes -Wsign-conversion'
lflags ='-lpthread -lm {}'.format('-static' if args.static else '')

# Run compiler
cmd = '{} {} {} {} -o {} {}'.format(args.compiler, cflags, wflags, sources, args.output, lflags)
print(cmd)
res = os.system(cmd)

# Run tests. Only if we compiled the test program, and compilation was successful
if res == 0 and args.test:
    print("running tests...")
    os.system(args.output)
