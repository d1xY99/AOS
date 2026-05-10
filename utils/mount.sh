#
# mount.sh
# mounts the AOS-flat.vmdk image file to linux
#
mkdir /mnt/aos
mkdir /mnt/aos/part0
mkdir /mnt/aos/part1
mkdir /mnt/aos/part2
mkdir /mnt/aos/part3
losetup /dev/loop4 ../aos-bin/AOS-flat.vmdk 
losetup -o 10321920 /dev/loop5 /dev/loop4
mount /dev/loop5 /mnt/aos/part1

