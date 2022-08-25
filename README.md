# Persistent Simple File System (PSFS)

PSFS is an on-disk EXT1 based persistent filesytem implementation.
It supports both file and directory based operations. 

## Data Structures

```c
/*  Structure for superblock */
typedef struct super_block {
	uint32_t magic_number;	            // File system magic number
	uint32_t blocks;	                // Number of blocks in file system (except super block)

	uint32_t inode_blocks;	            // Number of blocks reserved for inodes == 10% of Blocks
	uint32_t inodes;	                // Number of inodes in file system == length of inode bit map
	uint32_t inode_bitmap_block_idx;    // Block Number of the first inode bit map block
	uint32_t inode_block_idx;	        // Block Number of the first inode block

	uint32_t data_block_bitmap_idx;	    // Block number of the first data bitmap block
	uint32_t data_block_idx;	        // Block number of the first data block
	uint32_t data_blocks;               // Number of blocks reserved as data blocks
} super_block;
```

```c
/* This is the structure for inodes*/
typedef struct inode {
	uint32_t valid;            // 0 if invalid
	uint32_t size;             // logical size of the file
	uint32_t direct[5];        // direct data block pointer
	uint32_t indirect;         // indirect pointer
} inode;
```

```c
/* This is the structure written to directories */
typedef struct child {
    int valid;               // valid or not
    int type;                // directory or file
    char name[MAX_FILENAME]; // Maximum length of a directory/file name
    int length;              // length of the directory / file name
    int inumber;             // inode no of the directory / file
} child;
```

## API

```c
int format(disk *diskptr);

int mount(disk *diskptr);

int create_file();

int remove_file(int inumber);

int stat(int inumber);

int read_i(int inumber, char *data, int length, int offset);

int write_i(int inumber, char *data, int length, int offset);

int fit_to_size(int inumber, int size);
```