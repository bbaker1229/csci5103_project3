#include "fs.h"
#include "disk.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/param.h>

#define FS_MAGIC           0xf0f03410
#define INODES_PER_BLOCK   128
#define POINTERS_PER_INODE 5
#define POINTERS_PER_BLOCK 1024

#define FREE 				0
#define BUSY				1

bool fs_mounted = false;
char* freemap;

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

void inode_load(int inumber, struct fs_inode *inode)
{
	union fs_block inode_block;

	disk_read(1 + inumber / INODES_PER_BLOCK, inode_block.data);

	*inode = inode_block.inode[inumber % INODES_PER_BLOCK];
}

void inode_save(int inumber, struct fs_inode *inode)
{
	union fs_block inode_block;

	disk_read(1 + inumber / INODES_PER_BLOCK, inode_block.data);

	inode_block.inode[inumber % INODES_PER_BLOCK] = *inode;

	disk_write(1 + inumber / INODES_PER_BLOCK, inode_block.data);
}

int fs_format()
{
	union fs_block super_block;
	union fs_block empty_block;
	int i;

	// don't format if
	if ( fs_mounted ) {
		printf("fs_format: file system already mounted\n");
		return 0;
	}

	memset(empty_block.data, 0, sizeof(empty_block));

	super_block.super.magic = FS_MAGIC;
	super_block.super.nblocks = disk_size();
	super_block.super.ninodeblocks = disk_size() / 10 + 1;
	super_block.super.ninodes = 0;

	disk_write(0, super_block.data);

	for ( i = 1; i < disk_size(); i++ ) {
		disk_write(i, empty_block.data);
	}

	return 1;
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
	union fs_block super_block;
	union fs_block inode_block;
	union fs_block indirect_block;

	int i,j,k;

	// make sure there's not already a file system mounted
	if (fs_mounted ) {
		printf("fs_mount: file system already mounted\n");
		return 0;
	}

	disk_read(0,super_block.data);

	// make sure a file system exists on the disk
	if ( super_block.super.magic != FS_MAGIC ) {
		printf("fs_mount: file system invalid format\n");
		return 0;
	}

	// create an array for our free block bitmap and zero it out
	freemap = (char*) malloc(disk_size() * sizeof(char));
	memset(freemap, FREE, disk_size());

	// we at least have an occupied super block and some inode blocks
	memset(freemap, BUSY, 1 + super_block.super.ninodeblocks);

	// for each inode, figure out direct blocks, indirect blocks, and indirect data blocks
	for ( i = 0; i < super_block.super.ninodeblocks; i++ ) {
		disk_read((i + 1), inode_block.data);
		for ( j = 0; j < INODES_PER_BLOCK; j++ ) {
			if ( inode_block.inode[j].isvalid ) {
				for ( k = 0; k < POINTERS_PER_INODE; k++ ) {

					if ( inode_block.inode[j].direct[k] != 0 ) {
						// mark all used direct blocks as busy
						freemap[inode_block.inode[j].direct[k]] = BUSY;
					}
				}

				if ( inode_block.inode[j].indirect != 0 ) {
					// mark indirect block as busy
					freemap[inode_block.inode[j].indirect] = BUSY;

					disk_read(inode_block.inode[j].indirect, indirect_block.data);

					for (k = 0; k < POINTERS_PER_BLOCK; k++ ) {
						if ( indirect_block.pointers[k] != 0 ) {
							// mark indirect data blocks as busy
							freemap[indirect_block.pointers[k]] = BUSY;
						}
					}
				}
			}
		}
	}

	fs_mounted = true;

	return 1;
}

int fs_create()
{
	union fs_block super_block;
	struct fs_inode inode;
	int i;
	int inumber = -1;

	if ( !fs_mounted ) {
		printf("fs_create: can't create inode. no file system mounted\n");
		return -1;
	}

	disk_read(0, super_block.data);

	if ( super_block.super.ninodes >= super_block.super.ninodeblocks * INODES_PER_BLOCK ) {
		printf("fs_create: no free inode spaces\n");
		return -1;
	}

	// find a free inode slot
	for ( i = 0; i < super_block.super.ninodes; i++ ) {
		inode_load( i, &inode);
		if (!inode.isvalid) {
			// found a free slot, let's put our new inode there
			inumber = i;
			memset((char*)&inode, 0, sizeof(inode));
			inode.isvalid = 1;
			inode_save(inumber, &inode);

			// update super_block with new inode number
			super_block.super.ninodes++;
			disk_write( 0, super_block.data);
			break;
		}
	}

	return inumber;
}

int fs_delete( int inumber )
{
	union fs_block super_block;
	struct fs_inode inode;
	union fs_block indirect_block;
	union fs_block empty_block;
	int i;

	// validate file system mounted
	if ( !fs_mounted ) {
		printf("fs_delete: can't delete inode. no file system mounted\n");
		return 0;
	}

	// validate inumber
	disk_read(0, super_block.data);
	if (inumber >= super_block.super.ninodes) {
		printf("fs_delete: can't delete inode. inode number must be less than %d\n",
				super_block.super.ninodes);
		return 0;
	}

	memset(empty_block.data, 0, sizeof(empty_block));

	inode_load(inumber, &inode);

	// release direct data
	for (i = 0; i < POINTERS_PER_INODE; i++) {
		if ( inode.direct[i] != 0 ) {
			disk_write(inode.direct[i], empty_block.data);
			freemap[inode.direct[i]] = FREE;
		}
	}

	// check for indirect data
	if ( inode.indirect != 0 ) {
		disk_read(inode.indirect, indirect_block.data);

		// clear indirect data blocks
		for (i = 0; i < POINTERS_PER_BLOCK; i++) {
			if (indirect_block.pointers[i] != 0) {
				disk_write(indirect_block.pointers[i], empty_block.data);
				freemap[indirect_block.pointers[i]] = FREE;
			}
		}

		// clear the pointers themselves
		disk_write(inode.indirect, empty_block.data);
		freemap[indirect_block.pointers[i]] = FREE;
	}

	// update number of inodes
	super_block.super.ninodes--;
	disk_write(0, super_block.data);

	return 1;
}

int fs_getsize( int inumber )
{
	struct fs_inode inode;

	if (!fs_mounted) {
		printf("fs_getsize: no file system mounted\n");
		return -1;
	}

	inode_load(inumber, &inode);

	if (!inode.isvalid) {
		printf("fs_getsize: invalid inode number\n");
		return -1;
	}

	return inode.size;
}

int fs_read( int inumber, char *data, int length, int offset )
{
	struct fs_inode inode;
	int block_offset;
	int byte_offset;
	int block_number;
	int bytes_read;

	if (!fs_mounted) {
		printf("fs_read: no file system mounted\n");
		return -1;
	}

	inode_load(inumber, &inode);

	if (!inode.isvalid) {
		printf("fs_read: invalid inode number\n");
		return -1;
	}

	if (inode.size < offset) {
		printf("fs_read: invalid offset exceeds inode size: %d\n", inode.size);
		return -1;
	}

	// make sure we don't try to read past the end of the inode
	if (inode.size < offset + length ) {
		length = inode.size - offset;
	}

	// translate starting offset to block terms
	block_offset = offset / DISK_BLOCK_SIZE;
	byte_offset = offset % DISK_BLOCK_SIZE;

	do {
		union fs_block block;
		int bytes_to_read;

		// find the block pointer and read the block
		if ( block_offset < POINTERS_PER_INODE ) {
			block_number = inode.direct[block_offset];
		} else {
			disk_read(inode.indirect, block.data);
			block_number = block.pointers[block_offset - POINTERS_PER_INODE];
		}
		disk_read(block_number, block.data);

		// figure out how many bytes we need out of this block
		bytes_to_read = MIN(DISK_BLOCK_SIZE - byte_offset, length - bytes_read);

		// copy data into the output buffer
		strncpy(data + bytes_read, block.data + byte_offset, bytes_to_read);
		bytes_read += bytes_to_read;

		byte_offset = 0;
		block_offset++;

	} while ( bytes_read < length );

	return bytes_read;
}

int fs_write( int inumber, const char *data, int length, int offset )
{
	struct fs_inode inode;

	if (!fs_mounted) {
		printf("fs_write: no file system mounted\n");
		return -1;
	}

	inode_load(inumber, &inode);

	if (!inode.isvalid) {
		printf("fs_write: invalid inode number\n");
		return -1;
	}

	return 0;
}
