#ifndef _PSEUDONFS_H
#define _PSEUDONFS_H

#define _GNU_SOURCE

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/uaccess.h>
#include <linux/types.h>

#include "client.h"

#define S_IFDIR 0040000
#define S_IFREG 0100000

#endif