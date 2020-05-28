#!/usr/bin/python
import argparse, os

parser = argparse.ArgumentParser(description='c-chess-cli build script')
parser.add_argument('-c', '--compiler', help='Compiler', choices=['gcc', 'clang', 'musl-gcc',
    'musl-clang'], default='gcc')
parser.add_argument('-o', '--output', help='Output', default='./c-chess-cli')
parser.add_argument('-d', '--debug', action='store_true')
parser.add_argument('-s', '--static', action='store_true')
arguments = parser.parse_args()

cflags = '-std=gnu11 {} -mpopcnt -Ofast -flto'.format('-DNDEBUG -s' if not arguments.debug else '-g')
wflags = '-Wfatal-errors -Wall -Wextra -Wstrict-prototypes'
lflags ='-lpthread {}{}'.format('' if arguments.compiler[:4] == 'musl' else '-lm ',
    '-static' if arguments.static else '')
cmd = '{} {} {} ./*.c -o {} {}'.format(arguments.compiler, cflags, wflags, arguments.output, lflags)
print(cmd)
os.system(cmd)
