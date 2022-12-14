#!/bin/bash

SVNLSCHK=`realpath ./svnlschk`
COREDIR=`realpath ../cdroot/core`
HTMLFILE=`realpath ../website/index-nls.htm`
INSTALLNLS=`realpath ../files/floppy/nls`
NLSTEMPDIR=`realpath /tmp/sv_nls`
NLSARCH=`realpath ../website/nls.zip`

LANGSLIST="en      de     fr     it      pl     ru      si      sv      tr"
LANGSLONG="default german french italian polish russian slovene swedish turkish"

##############################################################################

processfile() {
  f=$1
  pkgname=$2
  nlstype=$3

  echo "Package: $pkgname"

  if [ $nlstype -eq 0 ] ; then
    LANGS=$LANGSLIST
    ENCOUNT=`$SVNLSCHK $f en $nlstype`
    unzip -Cj "$f" nls/\* -d "$NLSTEMPDIR/nls/"
  else
    LANGS=$LANGSLONG
    ENCOUNT=`$SVNLSCHK $f default $nlstype`
    if [ $nlstype -eq 1 ] ; then
      unzip -Cj "$f" source/command/strings/\*.lng -d "$NLSTEMPDIR/nls/command/"
    else
      unzip -Cj "$f" source/command/strings/\*.err -d "$NLSTEMPDIR/nls/command/"
    fi
  fi

  echo "<tr>" >> $HTMLFILE
  echo "  <td class=\"pkg\">$pkgname</td>" >> $HTMLFILE

  for l in $LANGS
  do

  if [ $ENCOUNT -lt 0 ] ; then
    echo "  <td class=\"err\">ERR</td>" >> $HTMLFILE
  fi

  if [ $ENCOUNT -eq 0 ] ; then
    echo "  <td class=\"unsup\"></td>" >> $HTMLFILE
  fi

  if [ $ENCOUNT -gt 0 ] ; then
    NLSCOUNT=`$SVNLSCHK $f $l $nlstype`
    if [ $ENCOUNT -eq $NLSCOUNT ] ; then
      echo "  <td class=\"complete\">$NLSCOUNT/$ENCOUNT</td>" >> $HTMLFILE
    else
      echo "  <td class=\"incomplete\">$NLSCOUNT/$ENCOUNT</td>" >> $HTMLFILE
    fi
  fi

  done

  echo "</tr>" >> $HTMLFILE
}

##############################################################################

cat head.html > $HTMLFILE

echo "<tr>" >> $HTMLFILE
printf "  <th></th>" >> $HTMLFILE

for l in $LANGSLIST
do
  printf "<th>%s</th>" "$l" >> $HTMLFILE
done

printf "\n</tr>\n" >> $HTMLFILE

mkdir -p "$NLSTEMPDIR/nls/command"

# process INSTALL
mkdir /tmp/nls
cp $INSTALLNLS/install.* /tmp/nls/
CURDIR=`pwd`
cd /tmp
zip -qq -r install.zip nls
rm -rf /tmp/nls
cd $CURDIR
processfile /tmp/install.zip "install" 0
rm /tmp/install.zip

# process COMMAND (special NLS format)
processfile $COREDIR/command.zip "command (lng)" 1
processfile $COREDIR/command.zip "command (err)" 2

for f in $COREDIR/*.zip
do

  pkgname=`basename $f .zip`
  # skip COMMAND
  if [ $pkgname = "command" ] ; then
  continue;
  fi

  processfile $f $pkgname 0

done

cat tail.html >> $HTMLFILE

cd "$NLSTEMPDIR"
zip -9 -k -r "$NLSARCH" nls
rm -rf "$NLSTEMPDIR"
cd "$CURDIR"

exit 0
