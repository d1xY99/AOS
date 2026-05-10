#ifdef AOS_MKIMG
#include "types.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>

#include "Dentry.h"
#include "FsContext.h"
#include "Superblock.h"
#include "MinixFsReg.h"
#include "MinixSuper.h"
#include "FsOps.h"
#include "MountPoint.h"

Superblock* superblock_;
FsContext* default_working_dir;


FsContext* getcwd() { return default_working_dir; }

// obviously NOT atomic, we need this for compatability in single threaded host code
size_t atomic_add(size_t& x,size_t y)
{
  x += y;
  return x-y;
}

int main(int argc, char *argv[])
{
  if (argc < 3 || argc % 2 == 0)
  {
    printf("Syntax: %s <filename of minixfs-formatted image> <offset in bytes> [file1-src file1-dest [file2-src file2-dest [....]]]\n",
        argv[0]);
    return -1;
  }

  FILE* image_fd = fopen(argv[1], "r+b");

  if (image_fd == 0)
  {
    printf("aos-mkimg: Error opening %s\n", argv[1]);
    return -1;
  }

  char* end;
  size_t offset = strtoul(argv[2],&end,10);
  if (strlen(end) != 0)
  {
    fclose(image_fd);
    printf("aos-mkimg: disk offset has to be a number!\n");
    return -1;
  }

  MinixFsReg* minixfs_type = new MinixFsReg();

  superblock_ = (Superblock*) new MinixSuper(minixfs_type, (size_t)image_fd, offset);
  Dentry *root = superblock_->getRoot();
  superblock_->setMountPoint(root);
  Dentry *mount_point = superblock_->getMountPoint();
  mount_point->setMountedRoot(mount_point);

  MountPoint vfs_dummy_(nullptr, mount_point, root, superblock_, 0);


  default_working_dir = new FsContext();
  Path root_path(root, &vfs_dummy_);
  default_working_dir->setRoot(root_path);
  default_working_dir->setPwd(root_path);

  for (int32 i = 2; i <= argc / 2; i++)
  {
    FILE* src_file = fopen(argv[2 * i - 1], "rb");

    if (src_file == 0)
    {
      printf("aos-mkimg: Failed to open host file %s\n", argv[2 * i - 1]);
      break;
    }

    fseek(src_file, 0, SEEK_END);
    size_t size = ftell(src_file);

    char *buf = new char[size];

    fseek(src_file, 0, SEEK_SET);
    assert(fread(buf, 1, size, src_file) == size && "aos-mkimg: fread was not able to read all bytes of the file");
    fclose(src_file);

    FsOps::rm(argv[2 * i]);
    int32 fd = FsOps::open(argv[2 * i], 4 | 8); // i.e. O_RDWR | O_CREAT
    if (fd < 0)
    {
      printf("aos-mkimg: Failed to open AOS file %s\n", argv[2 * i]);
      delete[] buf;
      continue;
    }
    int32 write_status = FsOps::write(fd, buf, size);
    if((size_t)write_status != size)
    {
      printf("aos-mkimg: Writing %s failed with retval %d (expected %zu)\n", argv[2 * i], write_status, size);
    }
    FsOps::close(fd);

    delete[] buf;
  }

  delete default_working_dir;
  delete superblock_;
  delete minixfs_type;
  fclose(image_fd);

  return 0;
}

#endif
