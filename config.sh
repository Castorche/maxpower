#!/bin/bash

THIS_SCRIPT=$BASH_SOURCE

if [ "$BASH_SOURCE" = "$0" ]
	then
		echo "This script must be sourced!"
		exit 1
fi


MAXPOWERDIR=`dirname $THIS_SCRIPT`

export MAXPOWERDIR=`readlink -e $MAXPOWERDIR`

echo Setting MAXPOWERDIR to $MAXPOWERDIR

export PYTHONPATH="$MAXPOWERDIR/utils:$PYTHONPATH"
