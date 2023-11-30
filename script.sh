#!/bin/bash

if [ "$#" -ne 1 ]
then
    echo "Mod de utilizare: $0 <caracter>"
    exit 1
fi

contor=0
while IFS= read -r line; do
    if [[ $line =~ ^[A-Z] ]]; then
        if [[ $line =~ $1 && $line =~ ^[A-Za-z0-9\ ,.!]*[.!?]$ && ! $line =~ ,\ .*[.!?]$ ]]
        then
            contor=$((contor+1))
        fi
    fi
done

exit $contor
