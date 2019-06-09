#!/bin/sh
BINDIR=$(dirname $(readlink -f "$0"))

usage() {
	echo "usage: $0 [-o] gamefilesdir objdir"
	echo "injects all gamescripts into game28.dta"
	echo "and all roomscripts into the resp. crm file"
	echo "if -o is given, the optimizer will be run first"
	exit 1
}

OPTIMIZER="python2 $BINDIR/optimizer.py"
do_optimize() {
	tmp=ags.optimizer.tmp.$$
	for opt in -cmp -pushpop -sourceline -lnl -ll -cmp2 -axmar -mrswap ; do
		$OPTIMIZER $opt < "$1" > $tmp
		test "$?" = 0 && mv $tmp "$1"
	done
	$BINDIR/agssemble "$1"
}

optimize=false
while getopts o name ; do
	case $name in
	o) optimize=true ;;
	?) usage ;;
	esac
done
shift $(($OPTIND - 1))

GAMEFILESDIR="$1"
OBJDIR="$2"

test -z "$OBJDIR" && usage

test -d "$OBJDIR" || {
	echo "error: objdir $OBJDIR not a directory" >&2
	exit 1
}

test -e "$GAMEFILESDIR"/game28.dta || {
	echo "error: "$GAMEFILESDIR"/game28.dta not found"
	exit 1
}

cnt=0
for i in globalscript dialogscript ; do
	$optimize && test -e "$OBJDIR"/"$i".s && do_optimize "$OBJDIR"/"$i".s
	test -e "$OBJDIR"/"$i".o || { echo "skipping $i" ; continue ; }
	"$BINDIR"/agsinject $cnt "$OBJDIR"/"$i".o "$GAMEFILESDIR"/game28.dta
	cnt=$(($cnt + 1))
done
for i in 0 `seq 9999` ; do
	i=gamescript"$i"
	$optimize && test -e "$OBJDIR"/"$i".s && do_optimize "$OBJDIR"/"$i".s
	test -e "$OBJDIR"/"$i".o || break
	"$BINDIR"/agsinject $cnt "$OBJDIR"/"$i".o "$GAMEFILESDIR"/game28.dta
	cnt=$(($cnt + 1))
done
for i in `seq 9999` ; do
	i=room"$i"
	$optimize && test -e "$OBJDIR"/"$i".s && do_optimize "$OBJDIR"/"$i".s
	test -e "$OBJDIR"/"$i".o && \
	"$BINDIR"/agsinject 0 "$OBJDIR"/"$i".o "$GAMEFILESDIR"/"$i".crm
done
