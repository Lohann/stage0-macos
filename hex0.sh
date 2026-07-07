#!/bin/sh

# USAGE 01:
# ./hex0.sh ./hello_macos-x86_64.hex0 > hello_world
# chmod +x ./hello_world
#
# without arguments, read from stdin by default:
# cat ./hello_macos-x86_64.hex0 | ./hex0.sh > hello_world
# chmod +x ./hello_world
#
# hex0 compiler - https://bootstrapping.miraheze.org/wiki/Stage0
# compile hex0 source in stdin to binary object at stdout
# [external] utilities used: printf
#
# hash or semicolon (#;) start a comment till the end of the line.
# remaining elements are expected to be two consecutive hex digits each,
# separated by space[s] and/or tab[s] and/or newline[s].
#
# backslash does not escape, and does not initiate line-continuation.
#
# Hexadecimal input are converted to octal using only shell builtins, hex
# format doesn't work in all shells (ex: dash) while octals is more portable.
# $ dash -c 'printf %b \\xFF' | od
# $ dash -c 'printf \\377' | od
#
# exit code is 1 if, after removing comments, an element is not two-hex-digits.
# exit code is 2 if printf failed.
# exit code is 3 if arguments are invalid.
# else exit code is 0
#
# Author: Lohann Paterno Coutinho Ferreira (developer@lohann.dev)

#### Be more Bourne compatible ####
# This is a modified version of m4sh from GNU autoconf v2.72
# original code:
# https://github.com/autotools-mirror/autoconf/blob/v2.72/lib/m4sugar/m4sh.m4
DUALCASE=1; export DUALCASE # for MKS sh
if test ${ZSH_VERSION+y} && (emulate sh) >/dev/null 2>&1
then emulate sh; NULLCMD=:
else case `(set -o) 2>/dev/null` in *posix*) set -o posix >/dev/null 2>&1 ;; *) : ;; esac
fi

# Exit script on error
set -e

# Consider read undefined variables an error
set -u

# Disable glob expansion, to avoid ambiguity when using IFS splitting.
case $- in
  *f*) : ;;
  *) set -f || { echo "builtin 'set -f' failed with status $?" >&2; exit 1; } ;;
esac

newline='
'       # \n newline 
tab='	' # \t tab
blk=' ' # \s space

# IFS needs to be set, to space, tab, and newline, in precisely that order.
# Quoting is to prevent editors from complaining about space-tab.
IFS=" $tab$newline"

# comment chars, when encountered must by skip all chars
# until next newline.
comment='#;'

# ascii chars to skip
skip=$IFS'!"$%&\'\''()*+,-./:<=>?@GHIJKLMNOPQRSTUVWXYZ[\\]^_`ghijklmnopqrstuvwxyz{|}~'

# Ensure predictable behavior from utilities with locale-dependent output.
LC_ALL=C; export LC_ALL
LANGUAGE=C; export LANGUAGE

# Ensure that fds 0, 1, and 2 are open.
if (exec 3>&0) 2>/dev/null; then :; else exec 0</dev/null; fi
if (exec 3>&1) 2>/dev/null; then :; else exec 1>/dev/null; fi
if (exec 3>&2)            ; then :; else exec 2>/dev/null; fi

# Parse user arguments, otherwise read from stdin
input_file=
case $# in
  0) input_file=-  ;;
  1) input_file=$1 ;;
  *) printf %s\\n "[ERROR] $0: extra operand '$2'" >&2; exit 3 ;;
esac

test "x$input_file" = x- || {
  test -e "$input_file" || { printf %s\\n "[ERROR] $0: not found '$input_file'" >&2; exit 3; }
  test -f "$input_file" || { printf %s\\n "[ERROR] $0: not a file '$input_file'" >&2; exit 3; }
  test -r "$input_file" || { printf %s\\n "[ERROR] $0: cannot read '$input_file'" >&2; exit 3; }
  # redirect file to stdin
  exec < "$input_file"  || { printf %s\\n "[ERROR] $0:$? failed to redirect '$input_file'
try: cat $input_file | $0 > $input_file.out" >&2; exit 3; }
}

p= # pattern
d= # hexadecimal digits
buf= # octal buffer
line= # source line
x= y= # temporary values
IFS=$newline
bytes=
while read -r line || test "x$line" != x
do
  # extract non-ascii bytes
  IFS=$skip${comment}0123456789ABCDEFabcdef x= y=
  for y in $line ''; do x=$x$y; done

  # remove non-ascii bytes.
  IFS=$skip$x x= y=
  for y in $line ''; do x=$x$y; done

  # skip lines that don't contain hexadecimal digits.
  case $x in
    [0123456789ABCDEFabcdef]*) line=$x ;;
    *) IFS=$newline; continue ;;
  esac

  # remove digits after '#' or ';'
  IFS=" $comment" x= y=
  for line in $line ''; do break; done

  # prepare for read next line
  IFS=$newline p= x= y=

  # Portably convert hexadecimal digits to octal digits
  while :
  do
    # Parse a single hexadecimal digit
    case $line in
      $p) p= ; break ;;
      $p[0]*) p=${p}0 d=${d}0 x=0 y=0 ;;
      $p[1]*) p=${p}1 d=${d}1 x=1 y=0 ;;
      $p[2]*) p=${p}2 d=${d}2 x=2 y=0 ;;
      $p[3]*) p=${p}3 d=${d}3 x=3 y=0 ;;
      $p[4]*) p=${p}4 d=${d}4 x=4 y=1 ;;
      $p[5]*) p=${p}5 d=${d}5 x=5 y=1 ;;
      $p[6]*) p=${p}6 d=${d}6 x=6 y=1 ;;
      $p[7]*) p=${p}7 d=${d}7 x=7 y=1 ;;
      $p[8]*) p=${p}8 d=${d}8 x=0 y=2 ;;
      $p[9]*) p=${p}9 d=${d}9 x=1 y=2 ;;
      $p[a]*) p=${p}a d=${d}A x=2 y=2 ;;
      $p[A]*) p=${p}A d=${d}A x=2 y=2 ;;
      $p[b]*) p=${p}b d=${d}B x=3 y=2 ;;
      $p[B]*) p=${p}B d=${d}B x=3 y=2 ;;
      $p[c]*) p=${p}c d=${d}C x=4 y=3 ;;
      $p[C]*) p=${p}C d=${d}C x=4 y=3 ;;
      $p[d]*) p=${p}d d=${d}D x=5 y=3 ;;
      $p[D]*) p=${p}D d=${d}D x=5 y=3 ;;
      $p[e]*) p=${p}e d=${d}E x=6 y=3 ;;
      $p[E]*) p=${p}E d=${d}E x=6 y=3 ;;
      $p[f]*) p=${p}f d=${d}F x=7 y=3 ;;
      $p[F]*) p=${p}F d=${d}F x=7 y=3 ;;
      *) printf '%s\n' "[ERROR] invalid line: '$line'" >&2; exit 3 ;;
    esac

    # Convert hexadecimal to octal
    case $d in
      ?) buf=$buf\\$y x= y= ;;
      [C840][01234567]) buf=${buf}0$x d= x= y= ;;
      [C840][89ABCDEF]) buf=${buf}1$x d= x= y= ;;
      [D951][01234567]) buf=${buf}2$x d= x= y= ;;
      [D951][89ABCDEF]) buf=${buf}3$x d= x= y= ;;
      [EA62][01234567]) buf=${buf}4$x d= x= y= ;;
      [EA62][89ABCDEF]) buf=${buf}5$x d= x= y= ;;
      [FB73][01234567]) buf=${buf}6$x d= x= y= ;;
      [FB73][89ABCDEF]) buf=${buf}7$x d= x= y= ;;
      *) : ;;
    esac
  done
done

# restore default IFS
IFS=" $tab$newline"

# exit if after removing comments
# an element is not two-hex-digits.
test "x$d" = x || exit 1

printf "$buf" || exit 2
