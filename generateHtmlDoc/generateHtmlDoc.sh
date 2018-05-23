#!/bin/bash

TEMP_DIR=~/tmp

#PUBLISH_HTML_DIR will be initialized automatically with the directory containing this script
PUBLISH_HTML_DIR=$(dirname $0)

echo $0

osName=$(uname -s)

CURRENT_DIRECTORY=$PWD

rm -rf $TEMP_DIR/CatraMMS.wiki
rm -rf $TEMP_DIR/www
rm -rf $PUBLISH_HTML_DIR/www

mkdir $TEMP_DIR/www
cd $TEMP_DIR
git clone https://github.com/giulianoc/CatraMMS.wiki.git

cd CatraMMS.wiki

#generate html and remove https://github.com/giulianoc/CatraMMS/wiki/
fileNumber=0
for filename in *.md; do
	fileBaseName=$(basename "$filename" .md)

	if [ "$fileBaseName" == "_Sidebar" ]; then
		cat $filename | pandoc -f gfm | sed -E "s/href=\"https:\/\/github.com\/giulianoc\/CatraMMS\/wiki\/([^\"]*)/target="\"main\"" href=\"\1.html/g" > $TEMP_DIR/www/$fileBaseName.html
	else
		cat $filename | pandoc -f gfm | sed -E "s/href=\"https:\/\/github.com\/giulianoc\/CatraMMS\/wiki\/([^\"]*)/href=\"\1.html/g" > $TEMP_DIR/www/$fileBaseName.html
	fi

	echo "$fileNumber: Generated $TEMP_DIR/www/$fileBaseName.html"
	fileNumber=$((fileNumber + 1))
done

#for link in *.md; do
	#linkToAddHtmlExtension=$(basename "$link" .md)

	#fileNumber=$((fileNumber - 1))
	#echo "$fileNumber: $linkToAddHtmlExtension ..."

	#for filename in *.md; do
		#fileBaseName=$(basename "$filename" .md)

		#if [ "$osName" == "Darwin" ]; then
			#gsed "s/$linkToAddHtmlExtension/$linkToAddHtmlExtension.html/gI" $TEMP_DIR/www/$fileBaseName.html > $TEMP_DIR/www/$fileBaseName.html.tmp
		#else
			#sed "s/$linkToAddHtmlExtension/$linkToAddHtmlExtension.html/gI" $TEMP_DIR/www/$fileBaseName.html > $TEMP_DIR/www/$fileBaseName.html.tmp
		#fi
		#mv $TEMP_DIR/www/$fileBaseName.html.tmp $TEMP_DIR/www/$fileBaseName.html
	#done
#done

cd $CURRENT_DIRECTORY

cp $PUBLISH_HTML_DIR/index.html $TEMP_DIR/www

#cd $TEMP_DIR/www
#for htmlFile in *.html; do
#	sed "s/a href=/a target="\"main\"" href=/g" $TEMP_DIR/www/$htmlFile > $TEMP_DIR/www/$htmlFile.tmp
#	mv $TEMP_DIR/www/$htmlFile.tmp $TEMP_DIR/www/$htmlFile
#done

#cd $CURRENT_DIRECTORY

mv $TEMP_DIR/www $PUBLISH_HTML_DIR/www

