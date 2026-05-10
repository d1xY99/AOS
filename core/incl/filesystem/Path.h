#pragma once

#include "types.h"
#include "ustring.h"

class Dentry;
class MountPoint;

class Path
{
public:
    Path() = default;
    Path(Dentry* dentry, MountPoint* mnt);
    Path(const Path&) = default;
    Path& operator=(const Path&) = default;
    bool operator==(const Path&) const;

    Path parent(const Path* global_root = nullptr) const;
    int child(const aostl::string& name, Path& out) const;

    aostl::string getAbsolutePath(const Path* global_root = nullptr) const;

    bool isGlobalRoot(const Path* global_root = nullptr) const;
    bool isMountRoot() const;

    Dentry* dentry_;
    MountPoint* mnt_;
};
