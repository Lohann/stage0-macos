#!/bin/sh

# USAGE: hex0.sh <input_file>
# 
# Example 1: read from file
# ```shell
# ./hex0.sh ./hello_macos-x86_64.hex0 > hello_world
# chmod +x ./hello_world
# ```

# Example 2: read from stdin
# ```shell
# cat ./hello_macos-x86_64.hex0 | ./hex0.sh > hello_world
# chmod +x ./hello_world
# ```
#
# Hex0 compiler - https://bootstrapping.miraheze.org/wiki/Stage0
# read hex0 source and output raw bytes to stdout
# [external] utilities used: none (except when the shell doesn't
# support 'printf', 'test', and 'echo' as shell's builtin).
#
# hash or semicolon (#;) start a comment till the end of the line.
# remaining elements are expected to be two consecutive hex digits each,
# separated by space[s] and/or tab[s] and/or newline[s].
#
# backslash does not escape, and does not initiate line-continuation.
#
# Hexadecimal input is converted to octal using only shell builtins, once hex
# format doesn't work in all shells (ex: dash) while octals are more portable.
# $ dash -c 'printf %b \\xFF' | od
# $ dash -c 'printf \\377' | od
#
# exit code is 0 on success
# exit code is 1 if, after removing comments, an element is not two-hex-digits.
# exit code is 2 if printf or shell builtin failed.
# exit code is 3 if arguments are invalid.
#
# Author: Lohann Paterno Coutinho Ferreira (developer@lohann.dev)

# change Bash and Zsh behavior to match Posix Shell Standard, the code below was 
# adapted from m4sh from GNU autoconf v2.72:
# https://github.com/autotools-mirror/autoconf/blob/v2.72/lib/m4sugar/m4sh.m4#L102-L115
DUALCASE=1; export DUALCASE # for MKS sh
if 'test' "${ZSH_VERSION+y}" && ('emulate' sh) >/dev/null 2>&1
then 'emulate' sh; NULLCMD=:
else case `('set' -o) 2>/dev/null` in *posix*) 'set' -o posix >/dev/null 2>&1 ;; *) : ;; esac
fi

# cleanup shell's hashtable and remove all aliases, temporarily modify 'PATH'
# to make sure we are executing a shell builtin instead an external tool.
x=${PATH-} y=${FPATH-}; PATH=/var/empty FPATH=
{ ('hash') >/dev/null 2>&1 && 'hash' -r; } || :
{ ('unalias' -a) >/dev/null 2>&1 && 'unalias' -a; } || :
PATH=$x FPATH=$y; x= y=

# IFS needs to be set, to space, tab, and newline, in precisely that order.
newline='
'       # \n newline 
tab='	' # \t tab
IFS=" $tab$newline"

# comment chars, when encountered must by skip all chars
# until next newline.
comment='#;'

# ascii chars to skip
skip=$IFS'!"$%&\'\''()*+,-./:<=>?@GHIJKLMNOPQRSTUVWXYZ[\\]^_`ghijklmnopqrstuvwxyz{|}~'

# ensure predictable behavior from utilities with locale-dependent output.
TZ=UTC; export TZ
LANG=C; export LANG
LC_ALL=C; export LC_ALL
LANGUAGE=C; export LANGUAGE

# unset variables known to interfere with behavior of common tools.
# ref: https://github.com/autotools-mirror/autoconf/blob/v2.72/lib/m4sugar/m4sh.m4#L507-L525
PS1='$ ' PS2='> ' PS4='+ '
{ test "${BASH_ENV+y}" && ( (unset BASH_ENV) || exit 1) >/dev/null 2>&1 && unset BASH_ENV; } || :
{ test "${ENV+y}" && ( (unset ENV) || exit 1) >/dev/null 2>&1 && unset ENV; } || :
{ test "${MAIL+y}" && ( (unset MAIL) || exit 1) >/dev/null 2>&1 && unset MAIL; } || :
{ test "${MAILPATH+y}" && ( (unset MAILPATH) || exit 1) >/dev/null 2>&1 && unset MAILPATH; } || :
{ test "${CDPATH+y}" && ( (unset CDPATH) || exit 1) >/dev/null 2>&1 && unset CDPATH; } || :
{ test "${GREP_OPTIONS+y}" && ( (unset GREP_OPTIONS) || exit 1) >/dev/null 2>&1 && unset GREP_OPTIONS; } || :
{ test "${CLICOLOR_FORCE+y}" && ( (unset CLICOLOR_FORCE) || exit 1) >/dev/null 2>&1 && unset CLICOLOR_FORCE; } || :

# Ensure that fds 0, 1, and 2 are open.
# ref: https://github.com/autotools-mirror/autoconf/blob/v2.72/lib/m4sugar/m4sh.m4#L309-L325
if (exec 3>&0) 2>/dev/null; then :; else exec 0</dev/null; fi
if (exec 3>&1) 2>/dev/null; then :; else exec 1>/dev/null; fi
if (exec 3>&2)            ; then :; else exec 2>/dev/null; fi

#   Shell Option  |                 Description
# -e | -o errexit | Exit immediately if a pipeline exits with a non-zero status.
# -u | -o nounset | Treat unset variables and parameters as an error when performing parameter expansion.
# -f | -o noglob  | Disable pathname expansion (to avoid ambiguity when using IFS splitting)
case $- in *e*) : ;; *) set -e || { echo "[ERROR] builtin 'set -e' failed with status $?" >&2; exit 2; } ;; esac
case $- in *u*) : ;; *) set -u || { echo "[ERROR] builtin 'set -u' failed with status $?" >&2; exit 2; } ;; esac
case $- in *f*) : ;; *) set -f || { echo "[ERROR] builtin 'set -f' failed with status $?" >&2; exit 2; } ;; esac

# Parse user arguments, otherwise read from stdin
input_file=
case $# in
  0) input_file=-  ;;
  1) input_file=$1 ;;
  *) printf %s\\n "[ERROR] $0: extra operand '$2'" >&2; exit 3 ;;
esac

# Check if input file exists
test "x$input_file" = x- || {
  test -e "$input_file" || { printf %s\\n "[ERROR] $0: not found '$input_file'" >&2; exit 3; }
  test -f "$input_file" || { printf %s\\n "[ERROR] $0: not a file '$input_file'" >&2; exit 3; }
  test -r "$input_file" || { printf %s\\n "[ERROR] $0: cannot read '$input_file'" >&2; exit 3; }
  # redirect file to stdin
  exec < "$input_file"  || { printf %s\\n "[ERROR] $0:$? failed to redirect '$input_file'
try: cat $input_file | $0 > $input_file.out" >&2; exit 3; }
}

buf= # octal buffer
line= # source line
flush= # buffer flush threshold
p= # pattern
d= # hexadecimal digits
x= y= z= # temporary values
IFS=$newline # read until newline (0x0A)
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
    *) line= x= y= IFS=$newline; continue ;;
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
      *) printf '%s\n' "[ERROR] $0: invalid line: '$line'" >&2; exit 3 ;;
    esac

    # Convert hexadecimal to octal
    case $d in
      ?) buf=$buf\\$y x= y=; continue ;;
      [C840][01234567]) buf=${buf}0$x d= x= y= ;;
      [C840][89ABCDEF]) buf=${buf}1$x d= x= y= ;;
      [D951][01234567]) buf=${buf}2$x d= x= y= ;;
      [D951][89ABCDEF]) buf=${buf}3$x d= x= y= ;;
      [EA62][01234567]) buf=${buf}4$x d= x= y= ;;
      [EA62][89ABCDEF]) buf=${buf}5$x d= x= y= ;;
      [FB73][01234567]) buf=${buf}6$x d= x= y= ;;
      [FB73][89ABCDEF]) buf=${buf}7$x d= x= y= ;;
      *) continue ;;
    esac

    # count number of bytes read to flush, arithmetic expansion is avoided once
    # it is not in posix shell standard neither supported by all shells (ex: ksh93).
    case $z in
      0) z=1 ;; 1) z=2 ;; 2) z=3 ;; 3) z=4 ;; 4) z=5 ;; 5) z=6 ;; 6) z=7 ;; 7) z=8 ;; # 0-8
      8) z=9 ;; 9) z=A ;; A) z=B ;; B) z=C ;; C) z=D ;; D) z=E ;; E) z=F ;; F) z=G ;; # 8-16
      G) z=H ;; H) z=I ;; I) z=J ;; J) z=K ;; K) z=L ;; L) z=M ;; M) z=N ;; N) z=O ;; # 16-32
      O) z=P ;; P) z=Q ;; Q) z=R ;; R) z=S ;; S) z=T ;; T) z=U ;; U) z=V ;; V) z=W ;; # 32-8
      W) printf "$buf" || { echo "[ERROR] $0: printf failed with status $?" >&2; exit 2; };  z=0 buf=  ;; # flush
      *) z=1 ;;
    esac
  done
  line= p= x= y=
done

# restore default IFS
IFS=" $tab$newline"

# exit if after removing comments
# an element is not two-hex-digits.
test "x$d" = x   || { echo "[ERROR] $0: odd number of hex digits" >&2; exit 1; }
test "x$buf" = x || printf "$buf" || { echo "[ERROR] $0: printf failed with status $?" >&2; exit 2; }
