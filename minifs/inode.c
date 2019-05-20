#include "constants.h"
#include "MINIFS_fs.h"
#include <linux/time.h>

extern struct file_operations MINIFS_fs_file_ops;

extern struct file_operations MINIFS_fs_dir_ops;

extern struct inode_operations MINIFS_fs_inode_ops;

extern struct address_space_operations MINIFS_fs_aops;

int MINIFS_write_inode(struct inode *inode, struct writeback_control *wbc)
{
    struct buffer_head *bh;
    struct MINIFS_inode *raw_inode = NULL;
    MINIFS_fs_get_inode(inode->i_sb, inode->i_ino, raw_inode);
    if (!raw_inode)
        return -EFAULT;
    raw_inode->mode = inode->i_mode;
    raw_inode->i_uid = fs_high2lowuid(i_uid_read(inode));
    raw_inode->i_gid = fs_high2lowgid(i_gid_read(inode));
    raw_inode->i_nlink = inode->i_nlink;
    raw_inode->file_size = inode->i_size;

    raw_inode->i_atime = (inode->i_atime.tv_sec);
    raw_inode->i_mtime = (inode->i_mtime.tv_sec);
    raw_inode->i_ctime = (inode->i_ctime.tv_sec);

    mark_buffer_dirty(bh);
    brelse(bh);
    return 0;
}

void MINIFS_evict_inode(struct inode *vfs_inode)
{
    struct super_block *sb = vfs_inode->i_sb;
    truncate_inode_pages_final(&vfs_inode->i_data);
    clear_inode(vfs_inode);
    if (vfs_inode->i_nlink)
    {
        return;
    }
    set_and_save_imap(sb, vfs_inode->i_ino, 0);
    return;
}

int MINIFS_fs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl)
{
    return MINIFS_fs_create_obj(dir, dentry, mode);
}

int MINIFS_fs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode)
{
    return MINIFS_fs_create_obj(dir, dentry, S_IFDIR | mode);
}

ssize_t MINIFS_write_inode_data(struct inode *inode, const void *buf, size_t count)
{
    struct super_block *sb;
    struct MINIFS_inode H_inode;

    sb = inode->i_sb;

    if (!buf)
    {
        return -EFAULT;
    }
    if (count > MINIFS_BLOCKSIZE * MINIFS_N_BLOCKS)
    {
        return -ENOSPC;
    }

    if (-1 == MINIFS_fs_get_inode(sb, inode->i_ino, &H_inode))
    {
        return -EFAULT;
    }
    if (count > MINIFS_BLOCKSIZE * H_inode.blocks)
    {
        int ret;
        ret = alloc_block_for_inode(sb, &H_inode,
                                    (count - MINIFS_BLOCKSIZE * H_inode.blocks) / MINIFS_BLOCKSIZE);
        if (ret)
        {
            return -EFAULT;
        }
        mark_inode_dirty(inode);
    }
    size_t count_res = count;
    int i;
    i = 0;
    while (count_res && i < MINIFS_N_BLOCKS)
    {
        struct buffer_head *bh;
        bh = sb_bread(sb, H_inode.block[i]);
        BUG_ON(!bh);
        size_t cpy_size;
        if (count_res >= MINIFS_BLOCKSIZE)
        {
            count_res -= MINIFS_BLOCKSIZE;
            cpy_size = MINIFS_BLOCKSIZE;
        }
        else
        {
            count_res = 0;
            cpy_size = count_res;
        }
        memcpy(bh->b_data, buf + i * MINIFS_BLOCKSIZE, cpy_size);
        map_bh(bh, sb, H_inode.block[i]);
        i++;
        brelse(bh);
    }
    while (i < H_inode.blocks)
    {
        struct buffer_head *bh;
        bh = sb_bread(sb, H_inode.block[i]);
        BUG_ON(!bh);
        memset(bh->b_data, 0, MINIFS_BLOCKSIZE);
        map_bh(bh, sb, H_inode.block[i]);
        brelse(bh);
        i++;
    }
    return count;
}

ssize_t MINIFS_read_inode_data(struct inode *inode, void *buf, size_t size)
{
    if (!buf)
    {
        return 0;
    }
    memset(buf, 0, size);
    struct super_block *sb = inode->i_sb;
    struct MINIFS_inode H_inode;
    if (-1 == MINIFS_fs_get_inode(sb, inode->i_ino, &H_inode))
    {
        return -EFAULT;
    }
    int i;
    for (i = 0; i < H_inode.blocks; ++i)
    {
        struct buffer_head *bh;
        bh = sb_bread(sb, H_inode.block[i]);
        BUG_ON(!bh);
        if ((i + 1) * MINIFS_BLOCKSIZE > size)
        {
            brelse(bh);
            return i * MINIFS_BLOCKSIZE;
        }
        memcpy(buf + i * (MINIFS_BLOCKSIZE), bh->b_data, MINIFS_BLOCKSIZE);
        brelse(bh);
    }
    return i * (MINIFS_BLOCKSIZE);
}

int MINIFS_fs_unlink(struct inode *dir, struct dentry *dentry)
{
    struct super_block *sb = dir->i_sb;
    struct MINIFS_inode H_dir_inode;
    if (MINIFS_fs_get_inode(sb, dir->i_ino, &H_dir_inode))
    {
        return -EFAULT;
    }
    ssize_t buf_size = H_dir_inode.blocks * MINIFS_BLOCKSIZE;
    void *buf = kmalloc(buf_size, GFP_KERNEL);
    if (MINIFS_read_inode_data(dir, buf, buf_size) != buf_size)
    {
        kfree(buf);
        return -EFAULT;
    }
    struct inode *inode = d_inode(dentry);
    int i;
    struct MINIFS_dir_record *p_dir;
    p_dir = (struct MINIFS_dir_record *)buf;
    for (i = 0; i < H_dir_inode.dir_children_count; ++i)
    {
        if (strncmp(dentry->d_name.name, p_dir[i].filename, MINIFS_FILENAME_MAX_LEN))
        {
            H_dir_inode.dir_children_count -= 1;

            struct MINIFS_dir_record *new_buf = kmalloc(buf_size - sizeof(struct MINIFS_dir_record), GFP_KERNEL);
            memcpy(new_buf, p_dir, (i) * sizeof(struct MINIFS_dir_record));
            memcpy(new_buf + i, p_dir + i + 1,
                   (H_dir_inode.dir_children_count - i - 1) * sizeof(struct MINIFS_dir_record));
            MINIFS_write_inode_data(dir, new_buf, buf_size - sizeof(struct MINIFS_dir_record));
            kfree(new_buf);
            break;
        }
    }
    inode_dec_link_count(inode);
    mark_inode_dirty(inode);
    kfree(buf);
    save_inode(sb, H_dir_inode);
    return 0;
}

int MINIFS_fs_create_obj(struct inode *dir, struct dentry *dentry, umode_t mode)
{
    struct super_block *sb = dir->i_sb;
    struct MINIFS_fs_super_block *disk_sb = sb->s_fs_info;
    const unsigned char *name = dentry->d_name.name;

    struct MINIFS_inode H_dir_inode;
    MINIFS_fs_get_inode(sb, dir->i_ino, &H_dir_inode);

    if (H_dir_inode.dir_children_count >= MINIFS_BLOCKSIZE / sizeof(struct MINIFS_dir_record))
    {
        return -ENOSPC;
    }

    uint64_t first_empty_inode_num = MINIFS_fs_get_empty_inode(dir->i_sb);
    BUG_ON(!first_empty_inode_num);
    struct inode *inode;
    struct MINIFS_inode raw_inode;
    inode = new_inode(sb);
    if (!inode)
    {
        return -ENOSPC;
    }
    inode->i_ino = first_empty_inode_num;
    raw_inode.inode_no = first_empty_inode_num;
    inode_init_owner(inode, dir, mode);
    inode->i_op = &MINIFS_fs_inode_ops;
    raw_inode.i_uid = i_uid_read(inode);
    raw_inode.i_gid = i_gid_read(inode);
    raw_inode.i_nlink = inode->i_nlink;
    struct timespec current_time;
    getnstimeofday(&current_time);

    raw_inode.i_atime = (inode->i_atime.tv_sec);
    raw_inode.i_ctime = (inode->i_ctime.tv_sec);
    raw_inode.i_mtime = (inode->i_mtime.tv_sec);

    raw_inode.mode = mode;
    if (S_ISDIR(mode))
    {
        inode->i_size = 1;
        inode->i_blocks = 1;
        inode->i_fop = &MINIFS_fs_dir_ops;

        raw_inode.blocks = 1;
        raw_inode.dir_children_count = 2;

        if (disk_sb->free_blocks <= 0)
        {
            return -ENOSPC;
        }
        struct MINIFS_dir_record dir_arr[2];
        uint64_t first_empty_block_num = MINIFS_fs_get_empty_block(sb);
        raw_inode.block[0] = first_empty_block_num;
        const char *cur_dir = ".";
        const char *parent_dir = "..";
        memcpy(dir_arr[0].filename, cur_dir, strlen(cur_dir) + 1);
        dir_arr[0].inode_no = first_empty_inode_num;
        memcpy(dir_arr[1].filename, parent_dir, strlen(parent_dir) + 1);
        dir_arr[2].inode_no = dir->i_ino;
        save_inode(sb, raw_inode);
        save_block(sb, first_empty_block_num, dir_arr, sizeof(struct MINIFS_dir_record) * 2);
        set_and_save_bmap(sb, first_empty_block_num, 1);

        disk_sb->free_blocks -= 1;
    }
    else if (S_ISREG(mode))
    {
        inode->i_size = 0;
        inode->i_blocks = 0;
        inode->i_fop = &MINIFS_fs_file_ops;
        inode->i_mapping->a_ops = &MINIFS_fs_aops;
        raw_inode.blocks = 0;
        raw_inode.file_size = 0;

        save_inode(sb, raw_inode);
    }
    struct MINIFS_dir_record new_dir;
    memcpy(new_dir.filename, name, strlen(name) + 1);
    new_dir.inode_no = first_empty_inode_num;
    struct buffer_head *bh;
    bh = sb_bread(sb, H_dir_inode.block[0]);
    memcpy(bh->b_data + H_dir_inode.dir_children_count * sizeof(struct MINIFS_dir_record), &new_dir, sizeof(new_dir));
    map_bh(bh, sb, H_dir_inode.block[0]);
    brelse(bh);

    H_dir_inode.dir_children_count += 1;
    save_inode(sb, H_dir_inode);

    set_and_save_imap(sb, first_empty_inode_num, 1);
    insert_inode_hash(inode);
    mark_inode_dirty(inode);
    mark_inode_dirty(dir);
    d_instantiate(dentry, inode);
    return 0;
}

int MINIFS_fs_get_inode(struct super_block *sb,
                        uint64_t inode_no, struct MINIFS_inode *raw_inode)
{
    if (!raw_inode)
    {
        return -1;
    }
    struct MINIFS_fs_super_block *H_sb = sb->s_fs_info;
    struct MINIFS_inode *H_inode_array = NULL;

    int i;
    struct buffer_head *bh;
    bh = sb_bread(sb,
                  H_sb->inode_table_block + inode_no * sizeof(struct MINIFS_inode) / MINIFS_BLOCKSIZE);
    BUG_ON(!bh);

    H_inode_array = (struct MINIFS_inode *)bh->b_data;
    int idx = inode_no % (MINIFS_BLOCKSIZE / sizeof(struct MINIFS_inode));
    ssize_t inode_array_size = MINIFS_BLOCKSIZE / sizeof(struct MINIFS_inode);
    if (idx > inode_array_size)
    {
        return -1;
    }
    memcpy(raw_inode, H_inode_array + idx, sizeof(struct MINIFS_inode));
    if (raw_inode->inode_no != inode_no)
    {
    }
    return 0;
}

void MINIFS_fs_convert_inode(struct MINIFS_inode *H_inode, struct inode *vfs_inode)
{
    vfs_inode->i_ino = H_inode->inode_no;
    vfs_inode->i_mode = H_inode->mode;
    vfs_inode->i_size = H_inode->file_size;
    set_nlink(vfs_inode, H_inode->i_nlink);
    i_uid_write(vfs_inode, H_inode->i_uid);
    i_gid_write(vfs_inode, H_inode->i_gid);
    vfs_inode->i_atime.tv_sec = H_inode->i_atime;
    vfs_inode->i_ctime.tv_sec = H_inode->i_ctime;
    vfs_inode->i_mtime.tv_sec = H_inode->i_mtime;
}

struct dentry *MINIFS_fs_lookup(struct inode *parent_inode,
                                struct dentry *child_dentry, unsigned int flags)
{
    struct super_block *sb = parent_inode->i_sb;
    struct MINIFS_inode H_inode;
    struct inode *inode = NULL;
    uint64_t data_block = 0, i;
    struct MINIFS_dir_record *dtptr;
    struct buffer_head *bh;

    if (-1 == MINIFS_fs_get_inode(sb, parent_inode->i_ino, &H_inode))
    {
        printk(KERN_ERR "MINIFS: cannot get inode\n");
        return ERR_PTR(-EFAULT);
    }

    data_block = H_inode.block[0];
    bh = sb_bread(sb, data_block);
    if (!bh)
    {
        return ERR_PTR(-EFAULT);
    }

    dtptr = (struct MINIFS_dir_record *)bh->b_data;

    for (i = 0; i < H_inode.dir_children_count; i++)
    {
        if (strncmp(child_dentry->d_name.name, dtptr[i].filename,
                    MINIFS_FILENAME_MAX_LEN) == 0)
        {

            inode = iget_locked(sb, dtptr[i].inode_no);
            if (!inode)
            {
                brelse(bh);
                return ERR_PTR(-EFAULT);
            }

            if (inode->i_state & I_NEW)
            {
                inode_init_owner(inode, parent_inode, 0);

                struct MINIFS_inode H_child_inode;
                if (-1 == MINIFS_fs_get_inode(sb, dtptr[i].inode_no, &H_child_inode))
                {
                    return ERR_PTR(-EFAULT);
                }

                MINIFS_fs_convert_inode(&H_child_inode, inode);
                inode->i_op = &MINIFS_fs_inode_ops;

                if (S_ISDIR(H_child_inode.mode))
                {
                    inode->i_fop = &MINIFS_fs_dir_ops;
                }
                else if (S_ISREG(H_child_inode.mode))
                {
                    inode->i_fop = &MINIFS_fs_file_ops;
                    ;
                    inode->i_mapping->a_ops = &MINIFS_fs_aops;
                }
                inode->i_mode = H_child_inode.mode;
                inode->i_size = H_child_inode.file_size;
                insert_inode_hash(inode);
                unlock_new_inode(inode);
            }

            d_add(child_dentry, inode);
            brelse(bh);

            return NULL;
        }
    }

    d_add(child_dentry, NULL);
    brelse(bh);
    return NULL;
}

int save_inode(struct super_block *sb, struct MINIFS_inode H_inode)
{
    uint64_t inode_num = H_inode.inode_no;
    struct MINIFS_fs_super_block *disk_sb = sb->s_fs_info;
    uint64_t block_idx = inode_num * sizeof(struct MINIFS_inode) / MINIFS_BLOCKSIZE + disk_sb->inode_table_block;
    uint64_t arr_off = inode_num % (MINIFS_BLOCKSIZE / sizeof(struct MINIFS_inode));

    struct buffer_head *bh;
    bh = sb_bread(sb, block_idx);
    BUG_ON(!bh);

    struct MINIFS_inode *p_disk_inode;
    p_disk_inode = (struct MINIFS_inode *)bh->b_data;
    memcpy(p_disk_inode + arr_off, &H_inode, sizeof(H_inode));

    map_bh(bh, sb, block_idx);
    brelse(bh);
    return 0;
}
