#
# umount.sh
# unmounts the AOS-flat.vmdk image file from linux
#
umount /mnt/aos/part1
losetup -d /dev/loop5
losetup -d /dev/loop4

