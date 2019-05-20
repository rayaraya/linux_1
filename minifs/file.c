#include "MINIFS_fs.h"
#include "constants.h"

int MINIFS_fs_readpage(struct file *file, struct page *page)
{
	return block_read_full_page(page, MINIFS_fs_get_block);
}

int MINIFS_fs_writepage(struct page *page, struct writeback_control *wbc)
{
	return block_write_full_page(page, MINIFS_fs_get_block, wbc);
}

int MINIFS_fs_write_begin(struct file *file, struct address_space *mapping,
			     loff_t pos, unsigned len, unsigned flags,
			     struct page **pagep, void **fsdata)
{
	int ret;
	ret = block_write_begin(mapping, pos, len, flags, pagep, MINIFS_fs_get_block);
	if (unlikely(ret))
		printk(KERN_INFO "MINIFS: Write failed for pos [%llu], len [%u]\n", pos, len);
	return ret;
}

int MINIFS_fs_iterate(struct file *filp, struct dir_context *ctx)
{
	struct MINIFS_inode H_inode;
	struct super_block *sb = filp->f_inode->i_sb;

	if (-1 == MINIFS_fs_get_inode(sb, filp->f_inode->i_ino, &H_inode))
		return -EFAULT;

	if (ctx->pos >= H_inode.dir_children_count)
	{
		return 0;
	}

	if (H_inode.blocks == 0)
	{
		return 0;
	}

	uint64_t i, dir_unread;
	dir_unread = H_inode.dir_children_count;
	if (dir_unread == 0)
	{
		return 0;
	}

	struct MINIFS_dir_record *dir_arr =
	    kmalloc(sizeof(struct MINIFS_dir_record) * dir_unread, GFP_KERNEL);

	struct buffer_head *bh;
	for (i = 0; (i < H_inode.blocks) && (dir_unread > 0); ++i)
	{
		bh = sb_bread(sb, H_inode.block[i]);
		uint64_t len = dir_unread * sizeof(struct MINIFS_dir_record);
		uint64_t off = H_inode.dir_children_count - dir_unread;
		if (len < MINIFS_BLOCKSIZE)
		{
			memcpy(dir_arr + (off * sizeof(struct MINIFS_dir_record)),
				bh->b_data, len);
			dir_unread = 0;
		}
		else
		{
			memcpy(dir_arr + (off * sizeof(struct MINIFS_dir_record)),
				bh->b_data, MINIFS_BLOCKSIZE);
			dir_unread -=
			    MINIFS_BLOCKSIZE / sizeof(struct MINIFS_dir_record);
		}
		brelse(bh);
	}
	for (i = 0; i < H_inode.dir_children_count; ++i)
	{
		dir_emit(ctx, dir_arr[i].filename, strlen(dir_arr[i].filename),
			  dir_arr[i].inode_no, DT_REG);
		ctx->pos++;
	}
	kfree(dir_arr);
	return 0;
}
