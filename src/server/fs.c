#include "fs.h"

#include <stdlib.h>

int fs_init(char *path, FS *fs)
{
    printf("fs_init %s\n", path);
    int fd = open(path, 0);
    if (fd < 0)
        return -1;
    
    struct stat st;
    if (fstat(fd, &st) < 0)
        return -1;

    fs->root = fd;
    fs->root_inode_n = st.st_ino;
    return 0;
}

void fs_clean(FS *fs)
{
    long n = sysconf(_SC_OPEN_MAX);
    for (long i = 5; i < n; i++)
        close(i);
}

int fs_find_object_by_inode_n_impl(int old_fd, int fd, ino_t inode_n, int *res)
{
    if (fd == 0)
        return -1;

    struct stat st;
    if (fstat(fd, &st) < 0)
    {
        printf("ERR: find_by_inode cant get stat\n");
        return -1;
    }

    printf("find_by_inode now in %lu\n", st.st_ino);

    if (st.st_ino == inode_n)
    {
        printf("find_by_inode found! %lu\n", st.st_ino);
        *res = fd;
        return 2;
    }

    if (S_ISDIR(st.st_mode))
    {
        if (fchdir(fd) < 0)
        {
            printf("ERR: find_by_inode cant change dir\n");
            return -1;
        }

        DIR *dir = fdopendir(fd);
        if (!dir)
        {
            printf("ERR: find_by_inode cant open dir\n");
            return -1;
        }

        struct dirent *ent;
        while (ent = readdir(dir))
        {
            if (strcmp(ent->d_name, ".") & strcmp(ent->d_name, ".."))
            {
                int new_fd = open(ent->d_name, 0);
                if (new_fd <= 0)
                {
                    printf("ERR: find_by_inode cant open fd\n");
                    return -1;
                }
                int rec = fs_find_object_by_inode_n_impl(fd, new_fd, inode_n, res);
                if (rec < 0)
                {
                    close(new_fd);
                    rewinddir(dir);
                    return -1;
                }
                else if (rec == 2)
                {
                    rewinddir(dir);
                    return 1;
                }
                else if (rec == 1)
                {
                    close(new_fd);
                    rewinddir(dir);
                    return 1;
                }
                close(new_fd);
            }
        }
        rewinddir(dir);
    }

    if (fchdir(old_fd) < 0)
    {
        printf("ERR: find_by_inode cant change dir\n");
        return -1;
    }
    return 0;
}

int fs_find_object_by_inode_n(FS *fs, ino_t inode_n)
{
    printf("find: %lu, root: %d\n", inode_n, fs->root);
    int res = 0;
    if (fs_find_object_by_inode_n_impl(fs->root, fs->root, inode_n, &res) < 0)
        return -1;
    printf("find: found: %d\n", res);
    return res;
}

int fs_handle_create(FS *fs, CreateRequest *req, CreateResponse *resp)
{
    printf("create\n");
    int parent_fd = fs_find_object_by_inode_n(fs, req->parent_inode_n);
    if (parent_fd <= 0)
        return -1;
    
    if (fchdir(parent_fd) < 0)
        return -1;

    int fd;
    struct stat st;

    switch (req->type)
    {
        case OBJECT_TYPE_FILE:
            fd = creat(req->name, 777);
            if (fd < 0)
                return -1;
            if (fstat(fd, &st) < 0)
                return -1;
            resp->inode_n = st.st_ino;
            break;
        
        case OBJECT_TYPE_DIR:
            if (mkdir(req->name, 777) < 0)
                return -1;
            fd = open(req->name, 0);
            if (fd < 0)
                return -1;
            if (fstat(fd, &st) < 0)
                return -1;
            resp->inode_n = st.st_ino;
            break;
    }
    printf("create: inode_n: %lu\n", resp->inode_n);
    if (fd != fs->root)
        close(fd);
    if (parent_fd != fs->root)
        close(parent_fd);
    return 0;
}

int fs_handle_link(FS *fs, LinkRequest *req, LinkResponse *resp)
{
    printf("link\n");
    int parent_fd = fs_find_object_by_inode_n(fs, req->parent_inode_n);
    if (parent_fd <= 0)
        return -1;

    int source_fd = fs_find_object_by_inode_n(fs, req->source_inode_n);
    if (source_fd <= 0)
        return -1;

    printf("link: name: %s, to_fd: %d\n", req->name, source_fd);

    if (linkat(source_fd, "", parent_fd, req->name, 0) < 0)
        return -1;
    
    if (parent_fd != fs->root)
        close(parent_fd);
    if (source_fd != fs->root)
        close(source_fd);
    return 0;
}

int fs_handle_unlink(FS *fs, UnlinkRequest *req, UnlinkResponse *resp)
{
    printf("unlink\n");
    int parent_fd = fs_find_object_by_inode_n(fs, req->parent_inode_n);
    if (parent_fd <= 0)
        return -1;

    if (fchdir(parent_fd) < 0)
        return -1;
    
    unlink(req->name);
    if (parent_fd != fs->root)
        close(parent_fd);
    return 0;
}

int fs_handle_read(FS *fs, ReadRequest *req, ReadResponse *resp)
{
    printf("read\n");
    int fd = fs_find_object_by_inode_n(fs, req->inode_n);
    if (fd <= 0)
    {
        printf("ERR (read): cant find fd\n");
        return -1;
    }

    resp->data.length = read(fd, resp->data.data, MAX_DATA_LENGTH);
    if (resp->data.length < 0)
    {
        printf("ERR (read): cant read\n");
        return -1;
    }

    printf("read: %d\n", resp->data.length);
    
    if (fd != fs->root)
        close(fd);
    return 0;
}

int fs_handle_write(FS *fs, WriteRequest *req, WriteResponse *resp)
{
    printf("write\n");
    int fd = fs_find_object_by_inode_n(fs, req->inode_n);
    if (fd <= 0)
    {
        printf("ERR (write): cant find fd\n");
        return -1;
    }

    printf("write: %d\n", req->data.length);

    if (write(fd, req->data.data, req->data.length) < 0)
    {
        printf("ERR (write): cant write\n");
        return -1;
    }
    
    if (fd != fs->root)
        close(fd);
    return 0;
}

int fs_handle_list(FS *fs, ListRequest *req, ListResponse *resp)
{
    printf("list\n");
    int fd = fs_find_object_by_inode_n(fs, req->inode_n);
    printf("list found fd\n");
    if (fd <= 0)
        return -1;
    
    if (fchdir(fd) < 0)
        return -1;
    
    DIR *dir = fdopendir(fd);
    if (!dir)
        return -1;
    
    struct dirent *ent;
    resp->objects.count = 0;
    while (ent = readdir(dir))
    {
        if (strcmp(ent->d_name, ".") & strcmp(ent->d_name, ".."))
        {
            ObjectType type;
            if (ent->d_type == DT_DIR)
                type = OBJECT_TYPE_DIR;
            else
                type = OBJECT_TYPE_FILE;

            resp->objects.objects[resp->objects.count].info = (ObjectInfo) { .inode_n = ent->d_ino, .type = type };
            strcpy(resp->objects.objects[resp->objects.count].name, ent->d_name);
            resp->objects.count++;
            if (resp->objects.count >= MAX_OBJECTS_COUNT)
                return -1;
        }
    }


    printf("list: %d objects\n", resp->objects.count);

    if (fd != fs->root)
    {
        closedir(dir);
    }
    else
    {
        rewinddir(dir);
    }
    return 0;
}

int fs_handle_rmdir(FS *fs, RmdirRequest *req, RmdirResponse *resp)
{
    printf("rmdir\n");
    int parent_fd = fs_find_object_by_inode_n(fs, req->parent_inode_n);
    if (parent_fd <= 0)
        return -1;

    if (fchdir(parent_fd) < 0)
        return -1;

    printf("rmdir: name: %s, parent_ino: %d\n", req->name, parent_fd);

    if (rmdir(req->name) < 0)
        return -1;
    
    if (parent_fd != fs->root)
        close(parent_fd);
    return 0;
}

int fs_handle_lookup(FS *fs, LookupRequest *req, LookupResponse *resp)
{
    printf("lookup: %s\n", req->name);
    int parent_fd = fs_find_object_by_inode_n(fs, req->parent_inode_n);
    if (parent_fd <= 0)
    {
        printf("ERR: lookup not found parent dir\n");
        return -1;
    }

    if (fchdir(parent_fd) < 0)
    {
        printf("ERR: lookup cant chdir\n");
        return -1;
    }

    int fd = open(req->name, 0);
    if (fd <= 0)
    {
        printf("ERR: lookup cant open\n");
        return -1;
    }
    
    struct stat st;
    if (fstat(fd, &st) < 0)
    {
        printf("ERR: lookup cant get stat\n");
        return -1;
    }

    ObjectType type;
    if (S_ISDIR(st.st_mode))
        type = OBJECT_TYPE_DIR;
    else
        type = OBJECT_TYPE_FILE;
    
    resp->info = (ObjectInfo) { .inode_n = st.st_ino, .type = type };


    printf("lookup: %lu\n", resp->info.inode_n);
    if (parent_fd != fs->root)
        close(parent_fd);
    if (fd != fs->root)
        close(fd);
    return 0;
}

int fs_handle_mount(FS *fs, MountRequest *req, MountResponse *resp)
{
    printf("mount\n");
    struct stat st;
    if (fstat(fs->root, &st) < 0)
        return -1;
    
    resp->inode_n = st.st_ino;
    printf("mount: %lu\n", resp->inode_n);

    return 0;
}

void fs_handle(FS *fs, MethodRequest *req, MethodResponse *resp)
{
    printf("\n----------\n");
    printf("fs_handle\n");
    int res = 0;
    resp->type = req->type;
    switch (req->type)
    {
        case METHOD_TYPE_CREATE:
            if (req->create.parent_inode_n == ROOT_DIR_INODE_N)
                req->create.parent_inode_n = fs->root_inode_n;
            res = fs_handle_create(fs, &req->create, &resp->create);
            if (resp->create.inode_n == fs->root_inode_n)
                resp->create.inode_n = ROOT_DIR_INODE_N;
            break;
        case METHOD_TYPE_LINK:
            if (req->link.parent_inode_n == ROOT_DIR_INODE_N)
                req->link.parent_inode_n = fs->root_inode_n;
            if (req->link.source_inode_n == ROOT_DIR_INODE_N)
                req->link.source_inode_n = fs->root_inode_n;
            res = fs_handle_link(fs, &req->link, &resp->link);
            break;
        case METHOD_TYPE_UNLINK:
            if (req->unlink.parent_inode_n == ROOT_DIR_INODE_N)
                req->unlink.parent_inode_n = fs->root_inode_n;
            res = fs_handle_unlink(fs, &req->unlink, &resp->unlink);
            break;
        case METHOD_TYPE_READ:
            if (req->read.inode_n == ROOT_DIR_INODE_N)
                req->read.inode_n = fs->root_inode_n;
            res = fs_handle_read(fs, &req->read, &resp->read);
            break;
        case METHOD_TYPE_WRITE:
            if (req->write.inode_n == ROOT_DIR_INODE_N)
                req->write.inode_n = fs->root_inode_n;
            res = fs_handle_write(fs, &req->write, &resp->write);
            break;
        case METHOD_TYPE_LIST:
            if (req->list.inode_n == ROOT_DIR_INODE_N)
                req->list.inode_n = fs->root_inode_n;
            res = fs_handle_list(fs, &req->list, &resp->list);
            for (uint16_t i=0; i<resp->list.objects.count; i++)
            {
                if (resp->list.objects.objects[i].info.inode_n == fs->root_inode_n)
                    resp->list.objects.objects[i].info.inode_n = ROOT_DIR_INODE_N;
            }
            break;
        case METHOD_TYPE_RMDIR:
            if (req->rmdir.parent_inode_n == ROOT_DIR_INODE_N)
                req->rmdir.parent_inode_n = fs->root_inode_n;
            res = fs_handle_rmdir(fs, &req->rmdir, &resp->rmdir);
            break;
        case METHOD_TYPE_LOOKUP:
            if (req->lookup.parent_inode_n == ROOT_DIR_INODE_N)
                req->lookup.parent_inode_n = fs->root_inode_n;
            res = fs_handle_lookup(fs, &req->lookup, &resp->lookup);
            if (resp->lookup.info.inode_n == fs->root_inode_n)
                resp->lookup.info.inode_n = ROOT_DIR_INODE_N;
            break;
        case METHOD_TYPE_MOUNT:
            res = fs_handle_mount(fs, &req->mount, &resp->mount);
            if (resp->mount.inode_n == fs->root_inode_n)
                resp->mount.inode_n = ROOT_DIR_INODE_N;
            break;
    }
    if (res < 0)
        resp->status = METHOD_STATUS_ERR;
    else
        resp->status = METHOD_STATUS_OK;
    
    fchdir(fs->root);
    printf("----------\n");
}