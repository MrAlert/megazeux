#ifndef __DIRENT_H
#define __DIRENT_H
#include <direct.h>

static inline int ux_mkdir (const char *path, int perm)
{
  return mkdir(path);
}

#define mkdir ux_mkdir

#endif /* __DIRENT_H */
