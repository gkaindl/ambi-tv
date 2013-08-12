#!/bin/bash
#
# from https://gist.github.com/azbesthu/3893319
 
REV=`zcat /usr/share/doc/raspberrypi-bootloader/changelog.Debian.gz | grep '* firmware as of' | head -n 1 | sed  -e 's/\  \*\ firmware as of \(.*\)$/\1/'`

echo "Installing kernel source for revision $REV..."
 
rm -rf rasp-tmp
mkdir -p rasp-tmp
mkdir -p rasp-tmp/linux
 
wget https://raw.github.com/raspberrypi/firmware/$REV/extra/git_hash -O rasp-tmp/git_hash
wget https://raw.github.com/raspberrypi/firmware/$REV/extra/Module.symvers -O rasp-tmp/Module.symvers
 
SOURCEHASH=`cat rasp-tmp/git_hash` 
 
wget https://github.com/raspberrypi/linux/tarball/$SOURCEHASH -O rasp-tmp/linux.tar.gz
 
cd  rasp-tmp
tar -xzf linux.tar.gz
 
OSVERSION=`uname -r`
 
mv raspberrypi-linux* /usr/src/linux-source-$OSVERSION
ln -s /usr/src/linux-source-$OSVERSION /lib/modules/$OSVERSION/build
 
cp Module.symvers /usr/src/linux-source-$OSVERSION/
 
zcat /proc/config.gz > /usr/src/linux-source-$OSVERSION/.config
 
cd /usr/src/linux-source-$OSVERSION/
make oldconfig
make prepare
make scripts
cd ~
