#!/bin/sh
BINDIR=$(dirname $(readlink -f "$0"))

usage() {
	echo "usage: $0 [-or] gamefilesdir objdir"
	echo "injects all gamescripts into game28.dta"
	echo "and all roomscripts into the resp. crm file"
	echo "if -r is given, the files will be recompiled first"
	echo "if -o is given, the optimizer will be run first (activates -r)"
	exit 1
}

OPTIMIZER="python2 $BINDIR/optimizer.py"
do_optimize_real() {
	echo "optimizing $1..."
	tmp=ags.optimizer.tmp.$$
	for opt in -cmp -pushpop -sourceline -lnl -ll -cmp2 -axmar -mrswap ; do
		$OPTIMIZER $opt < "$1" > $tmp
		test "$?" = 0 && mv $tmp "$1"
	done
}
do_recompile_real() {
	$BINDIR/agssemble "$1"
}

do_inject_real() {
	"$BINDIR"/agsinject "$1" "$2" "$3"
}

do_optimize() {
	$have_jobflow && printf "%s\n" "$1">&3 || do_optimize_real "$1"
}
do_recompile() {
	$have_jobflow && printf "%s\n" "$1">&3 || do_recompile_real "$1"
}
do_inject() {
	$have_jobflow && printf "%s:%s:%s\n" "$1" "$2" "$3">&3 \
	|| do_inject_real "$1" "$2" "$3"
}

test "$1" = --jfoptimize && {
	do_optimize_real "$2"
	exit $?
}
test "$1" = --jfrecompile && {
	do_recompile_real "$2"
	exit $?
}
test "$1" = --jfinject && {
	arg1=$(printf "%s\n" "$2"|cut -d : -f 1)
	arg2=$(printf "%s\n" "$2"|cut -d : -f 2)
	arg3=$(printf "%s\n" "$2"|cut -d : -f 3)
	do_inject_real "$arg1" "$arg2" "$arg3"
	exit $?
}

have_jobflow=false
type jobflow >/dev/null 2>&1 && have_jobflow=true

optimize=false
recompile=false
inject=true

while getopts o name ; do
	case $name in
	o) optimize=true ; recompile=true ;;
	r) recompile=true ;;
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

iterate_files() {
cnt=0
for i in globalscript dialogscript ; do
	$optimize && test -e "$OBJDIR"/"$i".s && do_optimize "$OBJDIR"/"$i".s
	$recompile && test -e "$OBJDIR"/"$i".s && do_recompile "$OBJDIR"/"$i".s
	$inject && {
		test -e "$OBJDIR"/"$i".o || { echo "skipping $i" ; continue ; }
		do_inject $cnt "$OBJDIR"/"$i".o "$GAMEFILESDIR"/game28.dta
	}
	cnt=$(($cnt + 1))
done
for i in 0 `seq 9999` ; do
	i=gamescript"$i"
	$optimize && test -e "$OBJDIR"/"$i".s && do_optimize "$OBJDIR"/"$i".s
	$recompile && test -e "$OBJDIR"/"$i".s && do_recompile "$OBJDIR"/"$i".s
	$inject && {
		test -e "$OBJDIR"/"$i".o || break
		do_inject $cnt "$OBJDIR"/"$i".o "$GAMEFILESDIR"/game28.dta
	}
	cnt=$(($cnt + 1))
done
for i in `seq 9999` ; do
	i=room"$i"
	$optimize && test -e "$OBJDIR"/"$i".s && do_optimize "$OBJDIR"/"$i".s
	$recompile && test -e "$OBJDIR"/"$i".s && do_recompile "$OBJDIR"/"$i".s
	$inject && {
		test -e "$OBJDIR"/"$i".o && \
		do_inject 0 "$OBJDIR"/"$i".o "$GAMEFILESDIR"/"$i".crm
	}
done
}

setup_pipe() {
        fd=$1
        node="$(mktemp -u)" || exit 1
        mkfifo -m0600 "$node" || exit 1
        eval "exec $fd<> $node"
        rm "$node"
}

jobflow_iterate() {
	setup_pipe 3
	jobflow -threads=$JOBS -eof=EOF -exec sh $0 "$1" {} <&3 &
	jfpid=$!
	iterate_files
	echo EOF>&3
	exec 3<&-
	wait $jfpid
}

JOBS=8
if ! $have_jobflow ; then
	iterate_files
else
	echo "executing in parallel with jobflow"
	trap "exit" INT TERM
	trap "kill 0" EXIT
	save_optimize=$optimize
	save_recompile=$recompile
	save_inject=$inject
	optimize=false
	recompile=false
	inject=false
	if $save_optimize ; then
		optimize=true
		jobflow_iterate --jfoptimize
		optimize=false
	fi
	if $save_recompile ; then
		recompile=true
		jobflow_iterate --jfrecompile
		recompile=false
	fi
	if $save_inject ; then
		inject=true
		# fixme: injections into game28.dta need to be sequential
		# only room injections can be done in parallel
		JOBS=1
		jobflow_iterate --jfinject
	fi
fi
