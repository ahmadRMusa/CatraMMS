#!/bin/bash

if [ $# -ne 1 ]
then
        echo "Usage $0 engine | encoder | api"

        exit
fi

component=$1

if [ "$component" == "engine" ]
then
        lastLogFile=$(find ~/logs/mmsEngineService -type f | sort -r | head -n 1)
elif [ "$component" == "encoder" ]
then
        lastLogFile=$(find ~/logs/mmsEncoder -type f | sort -r | head -n 1)
elif [ "$component" == "api" ]
then
        lastLogFile=$(find ~/logs/mmsAPI -type f | sort -r | head -n 1)
else
        echo "wrong input: $component"

        exit
fi

echo "$lastLogFile"

