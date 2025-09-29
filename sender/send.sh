#!/bin/bash
# Program:
# 	Send Secret Messages
# History:
# 	2023031000 Ashley
set -u
file='s_domain.txt'
#resolver=@163.22.2.1
#resolver=@9.9.9.9

i=1

while read line;do
    echo "dig $line $1"
	a=`dig +tcp $line $1`
	i=$((i+1))
done < $file
