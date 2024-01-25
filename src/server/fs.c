#include "fs.h"

int fs_init(char *path, FS *fs)
{
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

int fs_find_object_by_inode_n_impl(int old_fd, int fd, ino_t inode_n, int *res)
{
    if (fd == 0)
        return -1;

    fchdir(fd);

    struct stat st;
    if (fstat(fd, &st) < 0)
        return -1;

    if (st.st_ino == inode_n)
    {
        *res = fd;
        return 0;
    }

    if (S_ISDIR(st.st_mode))
    {
        DIR *dir = fdopendir(fd);
        if (!dir)
            return -1;

        struct dirent *ent;
        while (ent = readdir(dir))
        {
            if (strcmp(ent->d_name, ".") & strcmp(ent->d_name, ".."))
            {
                int new_fd = open(ent->d_name, 0);
                if (new_fd <= 0)
                    return -1;
                if (fs_find_object_by_inode_n_impl(fd, new_fd, inode_n, res) < 0)
                    return -1;
                if (*res != 0)
                    return 0;
                close(new_fd);
            }
        }
        closedir(dir);
    }

    fchdir(old_fd);
    return 0;
}

int fs_find_object_by_inode_n(FS *fs, ino_t inode_n)
{
    int res = 0;
    if (fs_find_object_by_inode_n_impl(fs->root, fs->root, inode_n, &res) < 0)
        return -1;
    return res;
}

int fs_handle_create(FS *fs, CreateRequest *req, CreateResponse *resp)
{
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
            fd = creat(req->name, 0);
            if (fd < 0)
                return -1;
            if (fstat(fd, &st) < 0)
                return -1;
            resp->inode_n = st.st_ino;
            break;
        
        case OBJECT_TYPE_DIR:
            fd = mkdir(req->name, S_IRWXU | S_IRWXG | S_IRWXO);
            if (fd < 0)
                return -1;
            if (fstat(fd, &st) < 0)
                return -1;
            resp->inode_n = st.st_ino;
            break;
    }
    if (fd != fs->root)
        close(fd);
    if (parent_fd != fs->root)
        close(parent_fd);
    return 0;
}

int fs_handle_link(FS *fs, LinkRequest *req, LinkResponse *resp)
{
    int parent_fd = fs_find_object_by_inode_n(fs, req->parent_inode_n);
    if (parent_fd <= 0)
        return -1;

    int source_fd = fs_find_object_by_inode_n(fs, req->source_inode_n);
    if (source_fd <= 0)
        return -1;

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
    int fd = fs_find_object_by_inode_n(fs, req->inode_n);
    if (fd <= 0)
        return -1;

    resp->data.length = read(fd, resp->data.data, MAX_DATA_LENGTH);
    if (resp->data.length < 0)
        return -1;
    
    if (fd != fs->root)
        close(fd);
    return 0;
}

int fs_handle_write(FS *fs, WriteRequest *req, WriteResponse *resp)
{
    int fd = fs_find_object_by_inode_n(fs, req->inode_n);
    if (fd <= 0)
        return -1;

    if (write(fd, req->data.data, req->data.length) < 0)
        return -1;
    
    if (fd != fs->root)
        close(fd);
    return 0;
}

int fs_handle_list(FS *fs, ListRequest *req, ListResponse *resp)
{
    int fd = fs_find_object_by_inode_n(fs, req->inode_n);
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

    if (fd != fs->root)
        close(fd);
    return 0;
}

int fs_handle_rmdir(FS *fs, RmdirRequest *req, RmdirResponse *resp)
{
    int parent_fd = fs_find_object_by_inode_n(fs, req->parent_inode_n);
    if (parent_fd <= 0)
        return -1;

    if (fchdir(parent_fd) < 0)
        return -1;

    if (rmdir(req->name) < 0)
        return -1;
    
    if (parent_fd != fs->root)
        close(parent_fd);
    return 0;
}

int fs_handle_lookup(FS *fs, LookupRequest *req, LookupResponse *resp)
{
    int parent_fd = fs_find_object_by_inode_n(fs, req->parent_inode_n);
    if (parent_fd <= 0)
        return -1;

    if (fchdir(parent_fd) < 0)
        return -1;

    int fd = open(req->name, 0);
    if (fd <= 0)
        return -1;
    
    struct stat st;
    if (fstat(fd, &st) < 0)
        return -1;

    ObjectType type;
    if (S_ISDIR(st.st_mode))
        type = OBJECT_TYPE_DIR;
    else
        type = OBJECT_TYPE_FILE;
    
    resp->info = (ObjectInfo) { .inode_n = st.st_ino, .type = type };

    if (parent_fd != fs->root)
        close(parent_fd);
    if (fd != fs->root)
        close(fd);
    return 0;
}

int fs_handle_mount(FS *fs, MountRequest *req, MountResponse *resp)
{
    struct stat st;
    if (fstat(fs->root, &st) < 0)
        return -1;
    
    resp->inode_n = st.st_ino;

    return 0;
}

void fs_handle(FS *fs, MethodRequest *req, MethodResponse *resp)
{
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
}