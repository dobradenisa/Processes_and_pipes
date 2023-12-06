#!/bin/bash

# verif nr de arg
if [ "$#" -ne 1 ]
then
    echo "Mod de utilizare: $0 <caracter>"
    exit 1
fi

contor=0
#citesc linie cu linie
while IFS= read -r line; do
    if [[ $line =~ ^[A-Z] ]]; then #verif daca incepe cu litera mare
        if [[ $line =~ $1 && $line =~ ^[A-Za-z0-9\ ,.!]*[.!?]$ && ! $line =~ ,\ .*[.!?]$ ]]
        then
            contor=$((contor+1))
        fi
    fi
done

exit $contor
