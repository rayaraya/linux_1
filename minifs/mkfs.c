#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <endian.h>
#include <linux/stat.h>

#include "constants.h"

static uint8_t *bmap;
static uint8_t *imap;
static uint64_t disk_size;
static uint64_t bmap_size;
static uint64_t imap_size;
static uint64_t inode_table_size;

struct MINIFS_fs_super_block
{
	uint64_t version;
	uint64_t magic;
	uint64_t block_size;
	uint64_t inodes_count;
	uint64_t free_blocks;
	uint64_t blocks_count;

	uint64_t bmap_block;
	uint64_t imap_block;
	uint64_t inode_table_block;
	uint64_t data_block_number;
	char padding[4016];
};
static struct MINIFS_fs_super_block super_block;

struct MINIFS_inode
{
	mode_t mode;
	uint64_t inode_no;
	uint64_t blocks;
	uint64_t block[MINIFS_N_BLOCKS];
	union {
		uint64_t file_size;
		uint64_t dir_children_count;
	};
	int32_t i_uid;
	int32_t i_gid;
	int32_t i_nlink;

	int64_t i_atime;
	int64_t i_mtime;
	int64_t i_ctime;
	char padding[112];
};

#define MINIFS_INODE_SIZE sizeof(struct MINIFS_inode)

struct MINIFS_dir_record
{
	char filename[MINIFS_FILENAME_MAX_LEN];
	uint64_t inode_no;
};

static off_t get_file_size(const char *path)
{
	off_t ret = -1;
	struct stat statbuf;
	if (stat(path, &statbuf) < 0)
	{
		return ret;
	}
	return statbuf.st_size;
}
static int set_bmap(uint64_t idx, int value)
{
	if (!bmap)
	{
		return -1;
	}
	uint64_t array_idx = idx / (sizeof(char) * 8);
	uint64_t off = idx % (sizeof(char) * 8);
	if (array_idx > bmap_size * MINIFS_BLOCKSIZE)
	{
		return -1;
	}
	if (value)
		bmap[array_idx] |= (1 << off);
	else
		bmap[array_idx] &= ~(1 << off);
	return 0;
}
static int init_disk(int fd, const char *path)
{
	disk_size = get_file_size(path);
	if (disk_size == -1)
	{
		return -1;
	}

	super_block.version = 1;
	super_block.block_size = MINIFS_BLOCKSIZE;
	super_block.magic = MAGIC_NUM;
	super_block.blocks_count = disk_size / MINIFS_BLOCKSIZE;

	super_block.inodes_count = super_block.blocks_count;
	super_block.free_blocks = 0;
	bmap_size = super_block.blocks_count / (8 * MINIFS_BLOCKSIZE);
	super_block.bmap_block = RESERVE_BLOCKS;

	if (super_block.blocks_count % (8 * MINIFS_BLOCKSIZE) != 0)
	{
		bmap_size += 1;
	}
	bmap = (uint8_t *)malloc(bmap_size * MINIFS_BLOCKSIZE);
	memset(bmap, 0, bmap_size * MINIFS_BLOCKSIZE);

	imap_size = super_block.inodes_count / (8 * MINIFS_BLOCKSIZE);
	super_block.imap_block = super_block.bmap_block + bmap_size;

	if (super_block.inodes_count % (8 * MINIFS_BLOCKSIZE) != 0)
	{
		imap_size += 1;
	}
	imap = (uint8_t *)malloc(imap_size * MINIFS_BLOCKSIZE);
	memset(imap, 0, imap_size * MINIFS_BLOCKSIZE);

	inode_table_size = super_block.inodes_count / (MINIFS_BLOCKSIZE / MINIFS_INODE_SIZE);
	super_block.inode_table_block = super_block.imap_block + imap_size;
	super_block.data_block_number = RESERVE_BLOCKS + bmap_size + imap_size + inode_table_size;
	super_block.free_blocks = super_block.blocks_count - super_block.data_block_number - 1;

	int idx;

	for (idx = 0; idx < super_block.data_block_number + 1; ++idx)
	{
		if (set_bmap(idx, 1))
		{
			return -1;
		}
	}

	return 0;
}
static int write_sb(int fd)
{
	ssize_t ret;
	ret = write(fd, &super_block, sizeof(super_block));
	if (ret != MINIFS_BLOCKSIZE)
	{
		return -1;
	}
	return 0;
}

static int write_bmap(int fd)
{
	ssize_t ret = -1;

	ret = write(fd, bmap, bmap_size * MINIFS_BLOCKSIZE);
	if (ret != bmap_size * MINIFS_BLOCKSIZE)
	{
		return -1;
	}
	return 0;
}
static int write_imap(int fd)
{
	memset(imap, 0, imap_size * MINIFS_BLOCKSIZE);
	imap[0] |= 0x3;

	ssize_t res = write(fd, imap, imap_size * MINIFS_BLOCKSIZE);
	if (res != imap_size * MINIFS_BLOCKSIZE)
	{
		return -1;
	}
	return 0;
}

static int write_itable(int fd)
{
	uint32_t _uid = getuid();
	uint32_t _gid = getgid();

	ssize_t ret;
	struct MINIFS_inode root_dir_inode;
	root_dir_inode.mode = S_IFDIR;
	root_dir_inode.inode_no = MINIFS_ROOT_INODE_NUM;
	root_dir_inode.blocks = 1;
	root_dir_inode.block[0] = super_block.data_block_number;
	root_dir_inode.dir_children_count = 3;
	root_dir_inode.i_gid = _gid;
	root_dir_inode.i_uid = _uid;
	root_dir_inode.i_nlink = 2;
	root_dir_inode.i_atime = root_dir_inode.i_mtime = root_dir_inode.i_ctime = ((int64_t)time(NULL));

	ret = write(fd, &root_dir_inode, sizeof(root_dir_inode));
	if (ret != sizeof(root_dir_inode))
	{
		return -1;
	}
	struct MINIFS_inode onefile_inode;
	onefile_inode.mode = S_IFREG;
	onefile_inode.inode_no = 1;
	onefile_inode.blocks = 0;
	onefile_inode.block[0] = 0;
	onefile_inode.file_size = 0;
	onefile_inode.i_gid = _gid;
	onefile_inode.i_uid = _uid;
	onefile_inode.i_nlink = 1;
	onefile_inode.i_atime = onefile_inode.i_mtime = onefile_inode.i_ctime = ((int64_t)time(NULL));

	ret = write(fd, &onefile_inode, sizeof(onefile_inode));
	if (ret != sizeof(onefile_inode))
	{
		return -1;
	}

	struct MINIFS_dir_record root_dir_c;
	const char *cur_dir = ".";
	const char *parent_dir = "..";

	memcpy(root_dir_c.filename, cur_dir, strlen(cur_dir) + 1);
	root_dir_c.inode_no = MINIFS_ROOT_INODE_NUM;
	struct MINIFS_dir_record root_dir_p;
	memcpy(root_dir_p.filename, parent_dir, strlen(parent_dir) + 1);
	root_dir_p.inode_no = MINIFS_ROOT_INODE_NUM;

	struct MINIFS_dir_record file_record;
	const char *onefile = "file";
	memcpy(file_record.filename, onefile, strlen(onefile) + 1);
	file_record.inode_no = 1;

	off_t current_off = lseek(fd, 0L, SEEK_CUR);

	if (-1 == lseek(fd, super_block.data_block_number * MINIFS_BLOCKSIZE, SEEK_SET))
	{
		return -1;
	}
	ret = write(fd, &root_dir_c, sizeof(root_dir_c));
	ret = write(fd, &root_dir_p, sizeof(root_dir_p));
	ret = write(fd, &file_record, sizeof(file_record));
	if (ret != sizeof(root_dir_c))
	{
		return -1;
	}
	return 0;
}
static int write_dummy(int fd)
{
	char dummy[MINIFS_BLOCKSIZE] = {0};
	ssize_t res = write(fd, dummy, MINIFS_BLOCKSIZE);
	if (res != MINIFS_BLOCKSIZE)
	{
		return -1;
	}
	return 0;
}

int main(int argc, char *argv[])
{
	int fd;
	ssize_t ret;

	if (argc != 2)
	{
		return -1;
	}

	fd = open(argv[1], O_RDWR);
	if (fd == -1)
	{
		return -1;
	}
	ret = 1;

	init_disk(fd, argv[1]);
	write_dummy(fd);
	write_sb(fd);
	write_bmap(fd);
	write_imap(fd);
	write_itable(fd);

	close(fd);
	return ret;
}
