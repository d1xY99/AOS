#include "FsContext.h"
#include "Dentry.h"
#include "kstring.h"
#include "assert.h"

FsContext::FsContext() :
    root_(), pwd_()
{
}

FsContext::FsContext(const FsContext& fsi) :
    root_(fsi.root_),pwd_(fsi.pwd_)
{
}

FsContext::~FsContext()
{
}
