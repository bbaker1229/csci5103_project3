
# csci5103_project3

File Systems

- Bryan Baker - bake1358@umn.edu
- Alice Anderegg - and08613@umn.edu
- Hailin Archer - deak0007@umn.edu

## Assumptions
- The write function needs to work only as intended by the shell program.
- The file will be written and not updated.
- The disk map used to track the locations of all free data blocks can be any type of data structure.  A byte array has been chosen.  

## How To Run
To build the files use `make`
To create a disk image run the command: `./simplefs image.xxx xxx` where xxx is the number of blocks you would like to create in the disk image.
Execute commands to the shell program to interact with the file system.  Use `help` to see a list of possibilities.  

## Function Definitions

### Main Functions (Required)

- **fs_format**:
    - Purpose:  Format the created disk image to use as a file system.
    - Input: None.
    - Output: a formatted disk image.
    - Return Value: 1 if successful, 0 otherwise.
    - Pseudo Code:
        - Check if mounted
        - Check if already formatted
        - Set and write superblock
        - Zero out the rest of the blocks of the file system.

- **fs_debug**:
    - Purpose: Print out the information of the file system.  The amount of inodes possible, the number of blocks, and the data for each valid inode in the file system.
    - Input: None.
    - Output: A screen printout of file system information.
    - Return Value: void.
    - Pseudo Code:
        - Check if formatted
        - Print the number of blocks, inode blocks, and inodes
        - Print information for each valid inode: the size, the direct data blocks used, the indirect data block, and the indirect data blocks used.

- **fs_mount**:
    - Purpose: Mount the disk image to use as a file system
    - Input: None.
    - Output: A mounted disk image for a file system.
    - Return Value: 1 if successful, 0 otherwise.
    - Pseudo Code:
        - Check if mounted
        - Check if formatted
        - Read the data on the disk image and create a disk map

- **fs_create**:
    - Purpose: Create an inode.
    - Input: None.
    - Output: A valid inode will be created on the file system to prepare to write.
    - Return Value: The created inode number, -1 otherwise.
    - Pseudo Code: 
        - Check if mounted
        - Check if inode table is full
        - Find a free inode slot
        - Write meta data to inode 
        - Return the created inode number

- **fs_delete**:
    - Purpose: Remove an inode from the file system.
    - Input: The inode number of delete.
    - Output: The inode will be removed from the file system.
    - Return Value: 1 if successful, 0 otherwise.
    - Pseudo Code: 
        - Check if mounted
        - Check if inode number is less than the valid range
        - Check if the inode number is valid
        - Write empty blocks to the direct nodes and update the disk map.
        - Check for indirect data and zero out if it exists.  Update the disk map.
        - Remove the inode number and save the information.

- **fs_getsize**:
    - Purpose: Get the amount of data associated with an inode.
    - Input: An inode number.
    - Output: The inode size.
    - Return Value: An integer for the inode size.
    - Pseudo Code: 
        - Check if mounted
        - Check is inode number is valid
        - Read and return the size of the inode.

- **fs_read**:
    - Purpose: Read data from the file system.
    - Input: The inode number, the data buffer, the length of data to read, and the offset.
    - Output: The data read from the file system.
    - Return Value: The number of bytes read, 0 otherwise.
    - Pseudo Code: 
        - Check if mounted
        - Check if inode number is valid
        - Determine which blocks and bytes to start reading from
        - Read through data starting with the direct nodes and then on to the indirect nodes if required
        - Copy to output buffer
        - Return the number of bytes read

- **fs_write**:
    - Purpose: Write data to an inode in the file system.
    - Input: The inode number, the data buffer, the length of the data to write, and the offset.
    - Output: The data will be written in to the file system.
    - Return Value: The number of bytes written, 0 otherwise.
    - Pseudo Code: 
        - Check if mounted
        - Check if inode number is valid
        - Determine which blocks to start writing to.
        - Find a free block to write to - return zero if disk is full
        - Find the number of bytes to write and read them from the buffer to a data block.
        - Write the data block to the disk and track using direct or indirect nodes.
        - Update the disk map
        - Update the inode size
        - Save the inode meta data
        - Return the number of bytes written

### Helper Functions (created by the team)
- **inode_load**:
    - Purpose: Find an inode block using an inode number.

- **inode_save**:
    - Purpose: Save an inode block using an inode number.

- **get_inode_cnt**:
    - Purpose: Get a count of all of the valid inodes on the file system.

- **find_free_block**:
    - Purpose: Returns the value of a free block which can be used to write data.
