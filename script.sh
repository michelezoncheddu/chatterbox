#
# @file   config.h
# @brief  File contenente alcune define con valori massimi utilizzabili
# @author Michele Zoncheddu 545227
# 
# Si dichiara che il contenuto di questo file e'
#   in ogni sua parte opera originale dell'autore
#

#!/bin/bash

# controllo il numero di parametri
if [ $# -lt 2 ]; then # less than
	echo "Usage: $0 DATA/chatty.conf* time"
	exit 1
fi

# cerco --help
for arg in "$@"; do
	if [ $arg == "-help" ]; then
		echo "Usage: $0 DATA/chatty.conf* time"
		exit 0
	fi
done

#controllo che $1 sia un file
if [ ! -f $1 ]; then
	echo "$1 is not a file"
	exit 1
fi

# imposto il file passato come parametro come stdin
exec 0<$1

# cerco DirName nel file
while read -a words; do
	if [ "${words[0]}" == "DirName" ]; then
		DirName=${words[2]}
	fi
done

# se il tempo e' 0
if [ $2 -eq 0 ]; then
	cd $DirName && ls
	exit 0
fi

# se il tempo e' negativo
if [ $2 -lt 0 ]; then
	echo "Time must be a positive value"
	exit 1
fi

# mi posiziono nella cartella
cd $DirName

# salvo i file da comprimere e cancellare
files=$(find . -mmin +$2 -type f)

# salvo le cartelle separatamente perche'
#   altrimenti il tar crea duplicati.
#   (quando comprime le cartelle,
#    aggiunge anche i file al loro interno,
#    che pero' sono gia' nella tarball)
folders=$(find . -mmin +$2 -type d)

# comprimo i file
tar -czvf archive.tar.gz $files

# cancello i file e le cartelle
if [ $? -eq 0 ]; then
	rm $files
	rm -d $folders
	exit 0
fi

exit 1
