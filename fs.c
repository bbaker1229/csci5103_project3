/*

csci5103_project3

File Systems

Bryan Baker - bake1358@umn.edu
Alice Anderegg - and08613@umn.edu
Hailin Archer - deak0007@umn.edu

*/

#include "fs.h"
#include "disk.h"

#include <stdio.h>
#include <math.h>
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

// Find an inode block using an inode number
void inode_load(int inumber, struct fs_inode *inode)
{
	union fs_block inode_block;
	disk_read(1 + (inumber / INODES_PER_BLOCK), inode_block.data);
	*inode = inode_block.inode[inumber % INODES_PER_BLOCK];
}

// Save an inode block using an inode number
void inode_save(int inumber, struct fs_inode *inode)
{
	union fs_block inode_block;
	disk_read(1 + (inumber / INODES_PER_BLOCK), inode_block.data);
	inode_block.inode[inumber % INODES_PER_BLOCK] = *inode;
	disk_write(1 + (inumber / INODES_PER_BLOCK), inode_block.data);
}

// Get a count of valid inodes saved to the disk
int get_inode_cnt()
{
	union fs_block super_block;
	union fs_block inode_block;
	int i, j;
	int cnt = 0;

	disk_read(0,super_block.data);

	for ( i = 0; i < super_block.super.ninodeblocks; i++ ) {
		disk_read((i + 1), inode_block.data);
		for ( j = 0; j < INODES_PER_BLOCK; j++ ) {
			if ( inode_block.inode[j].isvalid ) {
				cnt++;
			}
		}
	}
	return cnt;
}

// Find a free block to aid in writing data
int find_free_block()
{
	int i;

	for (i = 1; i < disk_size(); i++ ) {
		if (freemap[i] != BUSY) {
			return i;
		}
	}
	return -1;
}

// Format file system
int fs_format()
{
	union fs_block super_block;
	union fs_block empty_block;
	int i;
	char validate;
	int inode_val;

	// don't format if file system has been mounted
	if ( fs_mounted ) {
		printf("fs_format: file system already mounted\n");
		return 0;
	}

	// Read superblock
	disk_read(0,super_block.data);

	// Check if the file system has already been formatted and ask to reformat it.
	if (super_block.super.magic == FS_MAGIC) {
		printf("This disk has already been formated.\n");
		printf("Reformatting will erase the contents.\n");
		printf("Continue? (y/n) > ");
		scanf("%c", &validate);
		if (validate != 'y') {
			printf("fs_format: abort\n");
			return 0;
		}
	}

	memset(empty_block.data, 0, sizeof(empty_block));

	// Set attributes of superblock for the file system
	super_block.super.magic = FS_MAGIC;
	super_block.super.nblocks = disk_size();
	inode_val = ceil(disk_size() / 10);
	if (inode_val == 0) {
		inode_val = 1; // create at least 1 inode block
	}
	super_block.super.ninodeblocks = inode_val;
	super_block.super.ninodes = inode_val * INODES_PER_BLOCK;

	// Save the superblock to the file system
	disk_write(0, super_block.data);

	// Zero out all other datablocks.  
	for ( i = 1; i < disk_size(); i++ ) {
		disk_write(i, empty_block.data);
	}

	return 1;
}

// Debug function
void fs_debug()
{
	union fs_block super_block;
	union fs_block inode_block;
	union fs_block indirect_block;
	int i, j, k;

	// Read super block for attributes of file system.
	disk_read(0,super_block.data);

	// Check if the file system has been formatted.
	printf("superblock:\n");
	if (super_block.super.magic == FS_MAGIC) {
		printf("    magic number is valid\n");
	} else {
		printf("    magic number is invalid. aborting\n");
		return;
	}

	// Print the number of blocks, inode blocks, and inodes.
	printf("    %d blocks on disk\n",super_block.super.nblocks);
	printf("    %d inode blocks\n",super_block.super.ninodeblocks);
	printf("    %d inodes total\n",super_block.super.ninodes);

	// Print information on each valid inode
	for ( i = 0; i < super_block.super.ninodeblocks; i++ ) {
		disk_read((i + 1), inode_block.data);
		for ( j = 0; j < INODES_PER_BLOCK; j++ ) {
			if ( inode_block.inode[j].isvalid ) {
				printf("inode %d:\n", ( i * INODES_PER_BLOCK ) + j);
				printf("    size: %d bytes\n", inode_block.inode[j].size);
				printf("    direct blocks: ");
				for ( k = 0; k < POINTERS_PER_INODE; k++ ) {
					if ( inode_block.inode[j].direct[k] != 0 ) {
						printf("%d ", inode_block.inode[j].direct[k]);  // Printing the number of direct data blocks for a valid inode
					}
				}
				printf("\n");
				if ( inode_block.inode[j].indirect != 0 ) {
					printf("    indirect block: %d\n", inode_block.inode[j].indirect);
					disk_read(inode_block.inode[j].indirect, indirect_block.data);
					printf("    indirect data blocks: ");
					for (k = 0; k < POINTERS_PER_BLOCK; k++ ) {
						if ( indirect_block.pointers[k] != 0 ) {
							printf("%d ", indirect_block.pointers[k]);  // Printing the number of indirect data blocks for a valid inode if they exist.
						}
					}
					printf("\n");
				}
			}
		}
	}
}

// Mount file system
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

	// Read superblock data
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

// Create a valid inode
int fs_create()
{
	union fs_block super_block;
	struct fs_inode inode;
	int i;
	int inumber = -1;

	// Check if the file system has been mounted.
	if ( !fs_mounted ) {
		printf("fs_create: can't create inode. no file system mounted\n");
		return -1;
	}

	// Read the super block data
	disk_read(0, super_block.data);

	// If the maximum number of inodes have been created then exit
	if (get_inode_cnt() == super_block.super.ninodes) {
		printf("fs_create: can't create inode. inode table is full\n");
		return -1;
	}

	// find a free inode slot
	for ( i = 0; i < super_block.super.ninodeblocks * INODES_PER_BLOCK; i++ ) {
		inode_load( i, &inode);
		if (!inode.isvalid) {
			// found a free slot, let's put our new inode there
			inumber = i;
			memset((char*)&inode, 0, sizeof(inode));
			inode.isvalid = 1;
			inode.indirect = 0;
			inode_save(inumber, &inode);

			break;
		}
	}

	// Return the newly created inode number.
	return inumber;
}

// Delete an inode from the file system
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

	// Create an empty block of data to overwrite data being deleted.  
	memset(empty_block.data, 0, sizeof(empty_block));

	// Find the inode and make sure it is a valid inode
	inode_load(inumber, &inode);
	if (!inode.isvalid) {
		printf("fs_delete: can't delete inode. inode is not valid. Abort.\n");
		return 0;
	}

	// release direct data from freemap and overwrite with empty data
	for (i = 0; i < POINTERS_PER_INODE; i++) {
		if ( inode.direct[i] != 0 ) {
			disk_write(inode.direct[i], empty_block.data);
			freemap[inode.direct[i]] = FREE;
		}
	}

	// check for indirect data
	if ( inode.indirect != 0 ) {
		disk_read(inode.indirect, indirect_block.data);

		// clear indirect data blocks from freemap and overwrite with empty data
		for (i = 0; i < POINTERS_PER_BLOCK; i++) {
			if (indirect_block.pointers[i] != 0) {
				disk_write(indirect_block.pointers[i], empty_block.data);
				freemap[indirect_block.pointers[i]] = FREE;
			}
		}

		// clear the indirect pointers themselves from freemap and overwrite with empty data
		disk_write(inode.indirect, empty_block.data);
		freemap[indirect_block.pointers[i]] = FREE;
	}

	// delete the inode and save it
	memset(&inode, 0, sizeof(inode));
	inode_save(inumber, &inode);

	return 1;
}

// Get the amount of data associated with an inode
int fs_getsize( int inumber )
{
	struct fs_inode inode;

	// Check if the file system is mounted
	if (!fs_mounted) {
		printf("fs_getsize: no file system mounted\n");
		return -1;
	}

	inode_load(inumber, &inode);

	// Check if the inode is a valid node for the file system
	if (!inode.isvalid) {
		printf("fs_getsize: invalid inode number\n");
		return -1;
	}

	// Return the size of the data.
	return inode.size;
}

// read data from the file system
int fs_read( int inumber, char *data, int length, int offset )
{
	struct fs_inode inode;
	union fs_block super_block;
	int block_offset;
	int byte_offset;
	int block_number;
	int bytes_read = 0;

	// Check if the file system is mounted
	if (!fs_mounted) {
		printf("fs_read: no file system mounted\n");
		return 0;
	}

	// Read the super block
	disk_read(0, super_block.data);

	// Check is the inode value is less than the max number of inodes possible in the file system.
	if (inumber >= super_block.super.ninodes ) {
		printf("fs_read: invalid inode number must be less than %d\n",
				super_block.super.ninodes);
		return 0;
	}

	inode_load(inumber, &inode);

	// Check that the inode is valid.
	if (!inode.isvalid) {
		printf("fs_read: no inode data present for inode %d\n", inumber);
		return 0;
	}

	// return here if the offset doesn't make sense
	if ( offset >= inode.size ) {
		return 0;
	}

	// make sure we don't try to read past the end of the inode
	if (inode.size < offset + length ) {
		length = inode.size - offset;
	}

	// translate starting offset to block terms
	block_offset = offset / DISK_BLOCK_SIZE;
	byte_offset = offset % DISK_BLOCK_SIZE;

	while ( bytes_read < length ) {

		union fs_block block;
		int bytes_to_read;

		// find the block pointer and read the block
		if ( block_offset < POINTERS_PER_INODE ) {
			block_number = inode.direct[block_offset];  // direct inodes
		} else {
			disk_read(inode.indirect, block.data);
			block_number = block.pointers[block_offset - POINTERS_PER_INODE];  // indirect inodes 
		}
		disk_read(block_number, block.data);

		// figure out how many bytes we need out of this block
		bytes_to_read = MIN(DISK_BLOCK_SIZE - byte_offset, length - bytes_read);

		// copy data into the output buffer
		strncpy(data + bytes_read, block.data + byte_offset, bytes_to_read);
		bytes_read += bytes_to_read;

		byte_offset = 0;
		block_offset++;
	}

	return bytes_read;
}

// Write data to the file system.
int fs_write( int inumber, const char *data, int length, int offset )
{
	struct fs_inode inode;
	union fs_block super_block;
	union fs_block indirect_block;
	int block_offset = 0;
	int byte_offset;
	int bytes_written = 0;
	int i;

	// Check if the file system is mounted.
	if (!fs_mounted) {
		printf("fs_write: no file system mounted\n");
		return 0;
	}

	// Read the super block data
	disk_read(0, super_block.data);

	// Check that the inode requested is less than the max number of inodes in the file system.
	if (inumber >= super_block.super.ninodes ) {
		printf("fs_write: invalid inode number must be less than %d\n",
				super_block.super.ninodes);
		return 0;
	}

	inode_load(inumber, &inode);

	// Check that the inode is a valid inode
	if (!inode.isvalid) {
		printf("fs_write: no inode data present for inode %d\n", inumber);
		return 0;
	}

	// translate starting offset to block terms
	// Data will be written so we must find out the block offset to use for new data
	for (i = 0; i < POINTERS_PER_INODE; i++) {
		if (inode.direct[i] > 0) { // Take direct nodes into account
			block_offset++;
		}
	}
	if (inode.indirect) {
		disk_read(inode.indirect, indirect_block.data);
		for (i = 0; i < POINTERS_PER_BLOCK; i++) {
			if (indirect_block.pointers[i] > 0) {  // Take indirect nodes into account
				block_offset++;
			}
		}
	}

	// The offset is the cursor used while parsing through the buffer.
	byte_offset = offset;

	// Write while there are bytes to write
	while ( bytes_written < length ) {

		union fs_block data_block;
		int bytes_to_write;
		int write_block;

		// Find a free block to use when we want to write data to a block.
		write_block = find_free_block();

		// If there are no more free blocks the disk is full.
		if (write_block < 0) {
			printf("fs_write: disk is full\n");
			return 0;
		}

		// figure out how many bytes we need to write to this block
		bytes_to_write = MIN(DISK_BLOCK_SIZE, length - bytes_written);

		// copy data from the input buffer
		strncpy(data_block.data, data + bytes_written, bytes_to_write);

		// Fill direct inodes first
		if (block_offset < POINTERS_PER_INODE ) {
			inode.direct[block_offset] = write_block;
		} else { // Now fill indirect inodes
			if (!inode.indirect) {  // If there isn't an indirect block created, then create one
				inode.indirect = write_block;
				memset(indirect_block.data, 0, sizeof(indirect_block));
				disk_write(write_block,indirect_block.data);
				freemap[write_block] = BUSY;
				write_block = find_free_block(); // Find a new write block since we used ours to create the indirect block
				if (write_block < 0) {  // Check if disk is full
					printf("fs_write: disk is full\n");
					return 0;
				}
			}
			disk_read(inode.indirect, indirect_block.data); // Read indirect data block
			indirect_block.pointers[block_offset - POINTERS_PER_INODE] = write_block;
			disk_write(inode.indirect, indirect_block.data);  // Write the block number which will be used for data
		}
		disk_write (write_block, data_block.data);  // Write the data to the block chosen
		freemap[write_block] = BUSY;  // Mark the freemap as busy for the chosen block

		bytes_written += bytes_to_write;  // Track the number of bytes written to data blocks.

		byte_offset =+ byte_offset; // Increment the data cursor.
		block_offset++;  // Increment the number of blocks written to the file system for this inode.
	}
	// Keep track of the inode size and write the meta data to the file system.
	inode.size = inode.size + bytes_written;
	inode_save(inumber, &inode);
	return bytes_written;
}
