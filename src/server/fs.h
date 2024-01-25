#ifndef _FS_H
#define _FS_H

#define _GNU_SOURCE

#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <inttypes.h>

#define uint32_t uint32_t

#include "../shared/protocol.h"

#define MAX_PATH_SIZE 1024

typedef struct FS
{
    int root;
    ino_t root_inode_n;
} FS;

int fs_init(char *path, FS *fs);
void fs_handle(FS *fs, MethodRequest *req, MethodResponse *resp);

#endif