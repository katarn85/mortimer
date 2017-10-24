#!/bin/bash
index=1

echo "Select project to set active "
echo "----------------------------- "
prjs=$(ls ../project/)
for project in $prjs; do
	echo " $index. $project"
	projects[$index]="$project"
	index=$(($index+1))
done

echo " 0. EXIT"
echo "----------------------------- "

# number of devices   
size=$(( $index - 1 ))

# get device number    
read project_idx;

#check device number
if [ -z "$project_idx" ];then
	exit
elif [ $project_idx -eq 0 ];then
	exit
elif [ $project_idx -gt $size ]; then
	exit
fi

project_sel=${projects[$project_idx]}
echo "Selected project: $project_sel"

IFS='_' read -ra PROJECT_PARTS <<< "$project_sel"

echo " " > project.cfg
echo "#########################################################################" >> project.cfg
echo "# PROJECT SETTINGS" >> project.cfg
echo "#########################################################################" >> project.cfg
echo "export PROJECT_NAME = ${PROJECT_PARTS[0]}" >> project.cfg
echo "export PROJECT_VER  = ${PROJECT_PARTS[1]}" >> project.cfg

make init

