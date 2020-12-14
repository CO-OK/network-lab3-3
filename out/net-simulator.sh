#!/bin/zsh
#
#*************************************************************************
#Author:                  QV
#QQ:                      1260999370
#Date:                    2020-12-14
#Filename:                net-simulator.sh
#Description:             It's a script
#Copyright(C):            2020.All rights reseverd
#*************************************************************************
# Have a nice day.
set -u
if [ $# -ne 2 ]; then
		echo "wrong arg num, should be 2"
		exit 1
elif [ ${1} -eq 0 -a  ${2} -eq 0 ]; then
		sudo tc qdisc del dev wlp7s0 root netem
else
		sudo tc qdisc add dev wlp7s0 root netem delay ${1}ms
		sudo tc qdisc change dev wlp7s0 root netem loss ${2}%
fi
