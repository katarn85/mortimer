#!/bin/bash
set -e
PROGRAMS+="$(echo org.tizen.{clock,email,setting,taskmgr,download-manager})"
### Some of this is system services, that are not started via
### launch_app
#PROGRAMS+=$(cd /usr/apps && find|grep -E '\.exe$'|grep -v /_|sed -e "s#/bin/#.#g" -e "s#^\./##g"  -e "s#\.exe##g")
echo $PROGRAMS
for prog in $PROGRAMS
do
	printf $prog
	launch_app $prog __AUL_SDK__ DYNAMIC_ANALYSIS
	sleep 30
	last_part=$(echo $prog|sed -e 's#\.#/#g'|xargs basename)
	pid=$(ps aux|grep /apps/|grep $last_part)
	if [[ -z "$pid" ]] ; then
		echo "Error: $prog failed to start"
	else
		pid=$(echo $pid|python -c "print(raw_input().split()[1])")
		echo "Pid = $pid"
		if ! grep da_probe_osp.so /proc/$pid/maps ; then
			echo "Error: $prog is not preloaded."
		fi
		kill $pid
	fi
done

