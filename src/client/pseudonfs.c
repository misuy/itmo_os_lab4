#include "pseudonfs.h"


struct file_system_type pseudonfs_fs_type = { .name = "pseudonfs", .mount = pseudonfs_mount, .kill_sb = pseudonfs_kill_sb };

struct file_operations pseudonfs_dir_ops = {
    .iterate = pseudonfs_iterate,
    .read = pseudonfs_read,
    .write = pseudonfs_write,
};

struct inode_operations pseudonfs_inode_ops = {
    .lookup = pseudonfs_lookup,
    .create = pseudonfs_create,
    .mkdir = pseudonfs_mkdir,
    .rmdir = pseudonfs_rmdir,
    .link = pseudonfs_link,
    .unlink = pseudonfs_unlink,
};


int pseudonfs_iterate(struct file *f, struct dir_context *ctxt)
{
    struct inode *inode = f->f_inode;
    MethodRequest req;
    req.type = METHOD_TYPE_LIST;
    req.list = (ListRequest) { .inode_n = inode->i_ino };
    MethodResponse resp;
    if (call_method(inode->i_sb->s_fs_info, &req, &resp) < 0)
    {
        printk(KERN_ERR "iterate err\n");
        return -1;
    }
    if ((resp.status == METHOD_STATUS_ERR) | (resp.type != METHOD_TYPE_LIST))
    {
        printk(KERN_ERR "iterate call err\n");
        return -1;
    }

    while (ctxt->pos < resp.list.objects.count)
    {
        Object obj = resp.list.objects.objects[ctxt->pos];

        dir_emit(ctxt, obj.name, strlen(obj.name), obj.info.inode_n, obj.info.type == OBJECT_TYPE_DIR ? DT_DIR : DT_REG);
        ctxt->pos++;
    }
    return ctxt->pos;
}


ssize_t pseudonfs_read(struct file *f, char *buffer, size_t len, loff_t *off)
{
    MethodRequest req;
    req.type = METHOD_TYPE_READ;
    req.read = (ReadRequest) { .inode_n = f->f_inode->i_ino };
    MethodResponse resp;
    if (call_method(parent_inode->i_sb->s_fs_info, &req, &resp) < 0)
    {
        printk(KERN_ERR "read err\n");
        return -1;
    }
    if ((resp.status == METHOD_STATUS_ERR) | (resp.type != METHOD_TYPE_READ))
    {
        printk(KERN_ERR "read call err\n");
        return -1;
    }

    ssize_t ret = 0;
    while (*off < resp.read.data.length)
    {
        put_user(resp.read.data.data[*off], buffer + *off);
        ret++;
        (*off)++;
    }
    return ret;
}


ssize_t pseudonfs_write(struct file *f, const char *buffer, size_t len, loff_t *off)
{
    if (len > MAX_DATA_LENGTH)
        return -1;

    MethodRequest req;
    req.type = METHOD_TYPE_WRITE;
    req.write = (WriteRequest) { .inode_n = f->f_inode->i_ino };
    req.write.data.length = len;
    memset(req.write.data.data, 0, MAX_DATA_LENGTH);

    ssize_t ret = 0;
    while (*off < len)
    {
        get_user(req.write.data.data[*off], buffer + *off);
        ret++;
        (*off)++;
    }

    MethodResponse resp;
    if (call_method(parent_inode->i_sb->s_fs_info, &req, &resp) < 0)
    {
        printk(KERN_ERR "write err\n");
        return -1;
    }
    if ((resp.status == METHOD_STATUS_ERR) | (resp.type != METHOD_TYPE_WRITE))
    {
        printk(KERN_ERR "write call err\n");
        return -1;
    }

    return ret;
}




struct dentry * pseudonfs_lookup(struct inode *parent_inode, struct dentry *child_dentry, unsigned int flag)
{
    MethodRequest req;
    req.type = METHOD_TYPE_LOOKUP;
    req.lookup = (LookupRequest) { .parent_inode_n = parent_inode->i_ino, child_dentry->d_name.name };
    MethodResponse resp;
    if (call_method(parent_inode->i_sb->s_fs_info, &req, &resp) < 0)
    {
        printk(KERN_ERR "lookup err\n");
        return -1;
    }
    if ((resp.status == METHOD_STATUS_ERR) | (resp.type != METHOD_TYPE_LOOKUP))
    {
        printk(KERN_ERR "lookup call err\n");
        return -1;
    }
    struct inode *inode = pseudonfs_get_inode(parent_inode->i_sb, 0, (resp.lookup.info.type == OBJECT_TYPE_DIR ? S_IFDIR : S_IFREG) | 0x777, resp.lookup.info.inode_n);
    if (inode)
        d_add(child_dentry, inode);
    return child_dentry;
}


int pseudonfs_create(struct user_namespace *u_nmspc, struct inode *parent_inode, struct dentry *child_dentry, umode_t mode, bool b)
{
    MethodRequest req;
    req.type = METHOD_TYPE_CREATE;
    req.create = (CreateRequest) { .parent_inode_n = parent_inode->i_ino, .name = child_dentry->d_name.name, .type = OBJECT_TYPE_FILE };
    MethodResponse resp;
    if (call_method(parent_inode->i_sb->s_fs_info, &req, &resp) < 0)
    {
        printk(KERN_ERR "create err\n");
        return -1;
    }
    if ((resp.status == METHOD_STATUS_ERR) | (resp.type != METHOD_TYPE_CREATE))
    {
        printk(KERN_ERR "lookup call err\n");
        return -1;
    }

    struct inode *inode = pseudonfs_get_inode(parent_inode->i_sb, 0, S_IFREG | 0x777, resp.create.inode_n);
    if (inode)
        d_add(child_dentry, inode);
    
    return 0;
}


int pseudonfs_mkdir(struct user_namespace *u_nmspc, struct inode *parent_inode, struct dentry *child_dentry, umode_t mode)
{
    MethodRequest req;
    req.type = METHOD_TYPE_CREATE;
    req.create = (CreateRequest) { .parent_inode_n = parent_inode->i_ino, .name = child_dentry->d_name.name, .type = OBJECT_TYPE_DIR };
    MethodResponse resp;
    if (call_method(parent_inode->i_sb->s_fs_info, &req, &resp) < 0)
    {
        printk(KERN_ERR "mkdir err\n");
        return -1;
    }
    if ((resp.status == METHOD_STATUS_ERR) | (resp.type != METHOD_TYPE_CREATE))
    {
        printk(KERN_ERR "mkdir call err\n");
        return -1;
    }

    struct inode *inode = pseudonfs_get_inode(parent_inode->i_sb, 0, S_IFDIR | 0x777, resp.create.inode_n);
    if (inode)
        d_add(child_dentry, inode);
    
    return 0;
}


int pseudonfs_rmdir(struct inode *parent_inode, struct dentry *child_dentry)
{
    MethodRequest req;
    req.type = METHOD_TYPE_RMDIR;
    req.rmdir = (RmdirRequest) { .parent_inode_n = parent_inode->i_ino, .name = child_dentry->d_name.name };
    MethodResponse resp;
    if (call_method(parent_inode->i_sb->s_fs_info, &req, &resp) < 0)
    {
        printk(KERN_ERR "rmdir err\n");
        return -1;
    }
    if ((resp.status == METHOD_STATUS_ERR) | (resp.type != METHOD_TYPE_RMDIR))
    {
        printk(KERN_ERR "rmdir call err\n");
        return -1;
    }

    return 0;
}


int pseudonfs_link(struct dentry *old_dentry, struct inode *parent_inode, struct dentry *new_dentry)
{
    MethodRequest req;
    req.type = METHOD_TYPE_LINK;
    req.link = (LinkRequest) { .parent_inode_n = parent_inode->i_ino, .source_inode_n = old_dentry->d_inode->i_ino, .name new_dentry->d_name.name };
    MethodResponse resp;
    if (call_method(parent_inode->i_sb->s_fs_info, &req, &resp) < 0)
    {
        printk(KERN_ERR "link err\n");
        return -1;
    }
    if ((resp.status == METHOD_STATUS_ERR) | (resp.type != METHOD_TYPE_LINK))
    {
        printk(KERN_ERR "link call err\n");
        return -1;
    }
    
    return 0;
}


int pseudonfs_unlink(struct inode *parent_inode, struct dentry *child_dentry)
{
    MethodRequest req;
    req.type = METHOD_TYPE_UNLINK;
    req.unlink = (UnlinkRequest) { .parent_inode_n = parent_inode->i_ino, .name = child_dentry->d_name.name };
    MethodResponse resp;
    if (call_method(parent_inode->i_sb->s_fs_info, &req, &resp) < 0)
    {
        printk(KERN_ERR "unlink err\n");
        return -1;
    }
    if ((resp.status == METHOD_STATUS_ERR) | (resp.type != METHOD_TYPE_UNLINK))
    {
        printk(KERN_ERR "unlink call err\n");
        return -1;
    }
    
    return 0;
}




void pseudonfs_kill_sb(struct super_block *sb)
{
    ServerInfo *info = sb->s_fs_info;
    if (info != 0)
        kfree(info->ip);
    kfree(info);
    printk(KERN_INFO "killed superblock");
}


struct inode * pseudonfs_get_inode(struct super_block *sb, const struct inode *dir, umode_t mode, int i_ino)
{
    struct inode *inode;
    inode = new_inode(sb);
    if (inode != NULL) {
        inode->i_ino = i_ino;
        inode->i_op = &pseudonfs_inode_ops;
        inode->i_fop = &pseudonfs_dir_ops;
        inode_init_owner(&init_user_ns, inode, dir, mode);
    }
    return inode;
}


int pseudonfs_fill_super(struct super_block *sb, void *data, int silent)
{
    struct inode *inode;
    inode = pseudonfs_get_inode(sb, NULL, S_IFDIR | 0x777, ROOT_DIR_INODE_N);
    sb->s_root = d_make_root(inode);
    if (sb->s_root == NULL) {
        return -1;
    }
    return 0;
}


struct dentry * pseudonfs_mount(struct file_system_type *type, int flags, const char *addr, void *data)
{
    struct dentry *ret;
    uint16_t it = 0;
    while (addr[it] != 0)
    {
        if (addr[it] == ':')
            break;
        it++;
    }
    if (addr[it] == 0)
    {
        printk(KERN_ERR "bad addr\n");
        return 0;
    }

    ret = mount_nodev(type, flags, data, pseudonfs_fill_super);

    if (!ret)
        return 0;

    ret->d_sb->s_fs_info = 0;

    ServerInfo *info = kmalloc(sizeof(ServerInfo), GFP_KERNEL);
    info->ip = kmalloc(it + 1);
    memcpy(info->ip, addr, it);
    info->ip[it] = 0;

    sprintf(addr + it + 1, "%u", info->port);

    ret->d_sb->s_fs_info = (void *) info;

    printk(KERN_INFO "mounted\n");

    return ret;
}




int pseudonfs_init(void)
{
    printk(KERN_INFO "register pseudonfs\n");
    return register_filesystem(&pseudonfs_fs_type);
}


void pseudonfs_exit(void)
{
    printk(KERN_INFO "unregister pseudonfs\n");
    unregister_filesystem(&pseudonfs_fs_type);
}


module_init(pseudonfs_init);
module_exit(pseudonfs_exit);