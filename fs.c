#include "fs.h"
#include "disk.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#define FS_MAGIC           0xf0f03410
#define INODES_PER_BLOCK   128
#define POINTERS_PER_INODE 5
#define POINTERS_PER_BLOCK 1024

struct fs_superblock {
	int magic;
	int nblocks;
	int ninodeblocks;
	int ninodes;
};

struct fs_inode {
	int isvalid;
	int size;
	int direct[POINTERS_PER_INODE];
	int indirect;
};

union fs_block {
	struct fs_superblock super;
	struct fs_inode inode[INODES_PER_BLOCK];
	int pointers[POINTERS_PER_BLOCK];
	char data[DISK_BLOCK_SIZE];
};

int fs_format()
{
	return 0;
}

void fs_debug()
{
	union fs_block super_block;
	union fs_block inode_block;
	union fs_block indirect_block;
	int i, j, k;

	disk_read(0,super_block.data);

	printf("superblock:\n");
	if (super_block.super.magic == FS_MAGIC) {
		printf("    magic number is valid\n");
	} else {
		printf("    magic number is invalid. aborting\n");
		return;
	}

	printf("    %d blocks on disk\n",super_block.super.nblocks);
	printf("    %d inode blocks\n",super_block.super.ninodeblocks);
	printf("    %d inodes total\n",super_block.super.ninodes);

	for ( i = 0; i < super_block.super.ninodeblocks; i++ ) {
		disk_read((i + 1), inode_block.data);
		for ( j = 0; j < INODES_PER_BLOCK; j++ ) {
			if ( inode_block.inode[j].isvalid ) {
				printf("inode %d:\n", ( i * INODES_PER_BLOCK ) + j);
				printf("    size: %d bytes\n", inode_block.inode[j].size);
				printf("    direct blocks: ");
				for ( k = 0; k < POINTERS_PER_INODE; k++ ) {
					if ( inode_block.inode[j].direct[k] != 0 ) {
						printf("%d ", inode_block.inode[j].direct[k]);
					}
				}
				printf("\n");
				if ( inode_block.inode[j].indirect != 0 ) {
					printf("    indirect block: %d\n", inode_block.inode[j].indirect);
					disk_read(inode_block.inode[j].indirect, indirect_block.data);
					printf("    indirect data blocks: ");
					for (k = 0; k < POINTERS_PER_BLOCK; k++ ) {
						if ( indirect_block.pointers[k] != 0 ) {
							printf("%d ", indirect_block.pointers[k]);
						}
					}
					printf("\n");
				}
			}
		}
	}
}

int fs_mount()
{
	return 0;
}

int fs_create()
{
	return -1;
}

int fs_delete( int inumber )
{
	return 0;
}

int fs_getsize( int inumber )
{
	return -1;
}

int fs_read( int inumber, char *data, int length, int offset )
{
	return 0;
}

int fs_write( int inumber, const char *data, int length, int offset )
{
	return 0;
}
