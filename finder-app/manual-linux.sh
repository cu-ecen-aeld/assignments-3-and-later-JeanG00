#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-
SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    # TODO: Add your kernel build steps here
    echo "#### deep clean the kernel build tree - removing the .config file with any existing configurations"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
    echo "#### configure for "virt" arm dev board we will simulate in QEMU"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    echo "#### build a kernel image for booting in QEMU"
    # nproc => 2 4
    make -j"$(nproc)" ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
    echo "#### build any kernel modules"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules
    echo "#### build the devicetree"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs

    echo "########## KERNEL ${KERNEL_VERSION} BUILD DONE ##################"
fi

echo "Adding the Image in outdir"
cp "${OUTDIR}"/linux-stable/arch/${ARCH}/boot/Image "${OUTDIR}"
cp ${OUTDIR}/linux-stable/vmlinux ${OUTDIR}

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
mkdir -p $OUTDIR/rootfs
cd "$OUTDIR/rootfs"
mkdir bin dev etc home lib lib64 sys proc sbin tmp usr var
mkdir -p usr/bin usr/sbin 
mkdir -p var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    make distclean
    make defconfig
else
    cd busybox
fi

echo "#### Make and install busybox"
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install

echo "Library dependencies"
# readelf: Error: 'bin/busybox': No such file
cd "${OUTDIR}/rootfs"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

echo "#### Add library dependencies to rootfs"
cd "$SYSROOT"
cp lib/ld-linux-aarch64.so.1 "${OUTDIR}/rootfs/lib"
cp lib64/libm.so.6 "${OUTDIR}/rootfs/lib64"
cp lib64/libresolv.so.2 "${OUTDIR}/rootfs/lib64"
cp lib64/libc.so.6 "${OUTDIR}/rootfs/lib64"

echo "#### Make device nodes"
cd "$OUTDIR/rootfs" 
# https://github.com/torvalds/linux/blob/master/Documentation/admin-guide/devices.rst
# Syntax: mknod <name> <type> <major> <minor>
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 622 dev/tty1 c 4 2
sudo mknod -m 666 dev/console c 5 1

echo "####  Clean and build the writer utility in directory: ${FINDER_APP_DIR}"
cd ${FINDER_APP_DIR}
make clean
make

echo "#### Copy the finder related scripts and executables to the /home directory on the target rootfs"
cp writer "${OUTDIR}"/rootfs/home
cp ./*.sh "${OUTDIR}"/rootfs/home
cp -r conf/ "${OUTDIR}"/rootfs/home

echo "#### Create initramfs.cpio.gz"
# Issue: Kernel panic - not syncing: VFS: Unable to mount root fs on unknown-block(0,0)
# https://www.coursera.org/learn/linux-system-programming-introduction-to-buildroot/supplement/YGf42/assignment-3-part-2-instructions
# solved:  Make sure the .cpio file is created at the root directory of the target filesystem (not the root directory of the host filesystem, or any other directory on the host filesystem)
cd "$OUTDIR/rootfs"
find . | cpio -H newc -ov --owner root:root > initramfs.cpio
gzip -f < initramfs.cpio > initramfs.cpio.gz

echo "#### Chown the root directory"
cd "$OUTDIR"
cp rootfs/initramfs.cpio.gz .
sudo chown -R root:root rootfs

# TODO: Kernel panic - not syncing: Attempted to kill init! exitcode=0x00000000
# TODO: CPU: 0 PID: 1 Comm: sh Not tainted 5.15.163 #1