#!/usr/bin/python
import argparse, os

p = argparse.ArgumentParser(description='c-chess-cli build script')
p.add_argument('-c', '--compiler', help='Compiler', choices=['cc', 'gcc', 'clang', 'musl-gcc',
    'musl-clang'], default='cc')
p.add_argument('-o', '--output', help='Output file', default='')
p.add_argument('-d', '--debug', action='store_true', help='Debug compile')
p.add_argument('-s', '--static', action='store_true', help='Static compile')
p.add_argument('-p', '--program', help='Program to compile', choices=['main', 'test', 'engine'], default='main')
args = p.parse_args()

# Select source files, and set output file, depending on which program is being compiled
sources = 'src/bitboard.c src/gen.c src/position.c src/str.c src/util.c src/vec.c'
if args.program == 'main':
    sources += ' src/engine.c src/game.c src/main.c src/openings.c src/options.c src/sprt.c src/workers.c'
    if args.output == '': args.output = './c-chess-cli'
elif args.program == 'test':
    sources += ' test/test.c'
    if args.output == '': args.output = './test/test'
elif args.program == 'engine':
    sources += ' test/engine.c'
    if args.output == '': args.output = './test/engine'

# Determine flags for: compilation, warning, and linking
cflags = '-I./src -std=gnu11 {} -mpopcnt -Ofast -flto'.format('-DNDEBUG -s' if not args.debug else '-g')
wflags = '-Wfatal-errors -Wall -Wextra -Wstrict-prototypes -Wsign-conversion -Wshadow -Wpadded'
lflags ='-lpthread -lm'
if args.static: lflags += ' -static'
if 'ANDROID_DATA' in os.environ: lflags += ' -latomic'

# Run compiler
cmd = '{} {} {} {} -o {} {}'.format(args.compiler, cflags, wflags, sources, args.output, lflags)
print(cmd)
res = os.system(cmd)

# Run tests. Only if we compiled the test program, and compilation was successful
if res == 0 and args.program == 'test':
    print("running tests...")
    os.system(args.output)
