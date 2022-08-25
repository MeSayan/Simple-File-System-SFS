#ifndef SFS_FS_H
#define SFS_FS_H

#include <stdint.h>

/* Max length of a file/directory name*/
#define MAX_FILENAME 20 // max length of file/directory name
#define SFS_TYPE_D 1    // Type for directories
#define SFS_TYPE_F 0    // Type for files
#define MRD_Y 1         // create new root directory
#define MRD_N 0         // use existing root directory

const static uint32_t MAGIC = 12345;

typedef struct inode {
    uint32_t valid;     // 0 if invalid
    uint32_t size;      // logical size of the file
    uint32_t direct[5]; // direct data block pointer
    uint32_t indirect;  // indirect pointer
} inode;

typedef struct super_block {
    uint32_t magic_number; // File system magic number
    uint32_t blocks; // Number of blocks in file system (except super block)

    uint32_t
        inode_blocks; // Number of blocks reserved for inodes == 10% of Blocks
    uint32_t
        inodes; // Number of inodes in file system == length of inode bit map
    uint32_t
        inode_bitmap_block_idx; // Block Number of the first inode bit map block
    uint32_t inode_block_idx;   // Block Number of the first inode block

    uint32_t
        data_block_bitmap_idx; // Block number of the first data bitmap block
    uint32_t data_block_idx;   // Block number of the first data block
    uint32_t data_blocks;      // Number of blocks reserved as data blocks
} super_block;

/* This is the structure written to directories */
typedef struct child {
    int valid;               // valid or not
    int type;                // directory or file
    char name[MAX_FILENAME]; // Maximum length of a directory/file name
    int length;              // length of the directory / file name
    int inumber;             // inode no of the directory / file
} child;

int format(disk *diskptr);

int mount(disk *diskptr, int mount_roo_directory_flg);

int create_file();

int remove_file(int inumber);

int stat(int inumber);

int read_i(int inumber, char *data, int length, int offset);

int write_i(int inumber, char *data, int length, int offset);

int fit_to_size(int inumber, int size);

int read_file(char *filepath, char *data, int length, int offset);
int write_file(char *filepath, char *data, int length, int offset);
int create_dir(char *dirpath);
int remove_dir(char *dirpath);

void show_stats();

#endif