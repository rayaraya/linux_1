#include "MINIFS_fs.h"
#include "constants.h"

struct file_system_type MINIFS_fs_type = {
		.owner = THIS_MODULE,
		.name = "MINIFS_fs",
		.mount = MINIFS_fs_mount,
		.kill_sb = MINIFS_fs_kill_superblock,
};

const struct file_operations MINIFS_fs_file_ops = {
		.owner = THIS_MODULE,
		.llseek = generic_file_llseek,
		.mmap = generic_file_mmap,
		.fsync = generic_file_fsync,
		.read_iter = generic_file_read_iter,
		.write_iter = generic_file_write_iter,
};

const struct file_operations MINIFS_fs_dir_ops = {
		.owner = THIS_MODULE,
		.iterate = MINIFS_fs_iterate,
};

const struct inode_operations MINIFS_fs_inode_ops = {
		.lookup = MINIFS_fs_lookup,
		.mkdir = MINIFS_fs_mkdir,
		.create = MINIFS_fs_create,
		.unlink = MINIFS_fs_unlink,
};

const struct super_operations MINIFS_fs_super_ops = {
		.evict_inode = MINIFS_evict_inode,
		.write_inode = MINIFS_write_inode,
};

const struct address_space_operations MINIFS_fs_aops = {
		.readpage = MINIFS_fs_readpage,
		.writepage = MINIFS_fs_writepage,
		.write_begin = MINIFS_fs_write_begin,
		.write_end = generic_write_end,
};

int save_super(struct super_block *sb)
{
	struct MINIFS_fs_super_block *disk_sb = sb->s_fs_info;
	struct buffer_head *bh;
	bh = sb_bread(sb, 1);

	memcpy(bh->b_data, disk_sb, MINIFS_BLOCKSIZE);
	map_bh(bh, sb, 1);
	brelse(bh);
	return 0;
}

int MINIFS_fs_fill_super(struct super_block *sb, void *data, int silent)
{
	int ret = -EPERM;
	struct buffer_head *bh;
	bh = sb_bread(sb, 1);
	BUG_ON(!bh);
	struct MINIFS_fs_super_block *sb_disk;
	sb_disk = (struct MINIFS_fs_super_block *)bh->b_data;

	struct inode *root_inode;

	if (sb_disk->block_size != 4096)
	{
		ret = -EFAULT;
		goto release;
	}

	sb->s_magic = sb_disk->magic;
	sb->s_fs_info = sb_disk;
	sb->s_maxbytes = MINIFS_BLOCKSIZE * MINIFS_N_BLOCKS; /* Max file size */
	sb->s_op = &MINIFS_fs_super_ops;

	struct MINIFS_inode raw_root_node;
	MINIFS_fs_get_inode(sb, MINIFS_ROOT_INODE_NUM, &raw_root_node);

	root_inode = new_inode(sb);
	if (!root_inode)
		return -ENOMEM;

	inode_init_owner(root_inode, NULL, S_IFDIR | S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
	root_inode->i_sb = sb;
	root_inode->i_ino = MINIFS_ROOT_INODE_NUM;
	root_inode->i_atime = root_inode->i_mtime = root_inode->i_ctime =
			current_time(root_inode);

	root_inode->i_mode = raw_root_node.mode;
	root_inode->i_size = raw_root_node.dir_children_count;

	inc_nlink(root_inode);

	root_inode->i_op = &MINIFS_fs_inode_ops;
	root_inode->i_fop = &MINIFS_fs_dir_ops;

	sb->s_root = d_make_root(root_inode);
	if (!sb->s_root)
		return -ENOMEM;
release:
	brelse(bh);

	return 0;
}

struct dentry *MINIFS_fs_mount(struct file_system_type *fs_type, int flags,
															 const char *dev_name, void *data)
{
	struct dentry *ret;

	ret = mount_bdev(fs_type, flags, dev_name, data, MINIFS_fs_fill_super);

	if (IS_ERR(ret))
		printk(KERN_ERR "Error mounting MINIFS_fs\n");

	return ret;
}

void MINIFS_fs_kill_superblock(struct super_block *s)
{
	kill_block_super(s);
}

int MINIFS_fs_init(void)
{
	int ret;
	ret = register_filesystem(&MINIFS_fs_type);
	return ret;
}

void MINIFS_fs_exit(void)
{
	int ret;
	ret = unregister_filesystem(&MINIFS_fs_type);
}

module_init(MINIFS_fs_init);
module_exit(MINIFS_fs_exit);

