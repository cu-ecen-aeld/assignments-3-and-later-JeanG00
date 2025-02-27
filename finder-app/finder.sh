#!/bin/sh
#echo "isnide finder"
filesdir=$1
searchstr=$2

if ! [ $# -gt 1 ]
then 
	echo "Not enough parameters"
	exit 1
fi

if ! [ -d "${filesdir}" ]
then 
	echo "Directory doesnt exist"
	exit 1
fi

linecount=$(grep -rw  "${filesdir}" -e "${searchstr}" | wc -l)
filecount=$(find "${filesdir}" -type f | wc -l)
echo "The number of files are ${filecount} and the number of matching lines are ${linecount}"
