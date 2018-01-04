#! /bin/bash
#
# rearrange folders to get a working standalone Windows distro 
#
# command line args: 
# 1) full (absolute) installation path

echo "installing to: $1"

# strip executables
for i in pd.exe pd.com pd.dll; do
	strip --strip-unneeded -R .note -R .comment $1/bin/$i
done
# move folders from /lib/pd into top level directory
for i in doc extra po tcl; do
	cp -r $1/lib/pd/$i $1
done
# clear /lib folder (but keep it for later)
rm -r $1/lib/*
# we don't need the /share folder
rm -r $1/share
# untar pdprototype.tgz
tar -xzf ./pdprototype.tgz
# copy DLLs
cp -r ./pd/bin/* $1/bin
#copy Tcl/Tk
cp -r ./pd/lib/* $1/lib
# clean up
rm -r ./pd
