#!/bin/bash
function compactRef {
	workDir=${PWD}
	refDir=$1
	refDirgz=$2
	cp -r $refDir $refDirgz
	cd $refDirgz 
	basename=$( ls *.fa )
	ls -l *.bwt | cut -f5 -d' ' >"$basename"".bwt.gz.size"
	gzip *.bwt *.sa
	rm $basename
	touch $basename
	cd $workDir
}
inputDir=$1
outputDir=$2
refs=()
mkdir -p $outputDir
find $inputDir -mindepth 1 -maxdepth 1 -type d -print0 >tmpfile
while IFS=  read -r -d $'\0'; do
    refs+=("$REPLY")
done <tmpfile
rm tmpfile
for ref in "${refs[@]}"; do
 newref="$outputDir/${ref##*/}"
 echo $ref $newref
 compactRef $ref $newref
done


