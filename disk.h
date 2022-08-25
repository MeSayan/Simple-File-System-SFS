#ifndef SFS_DISK_H
#define SFS_DISK_H

#include <stdint.h>
#include <stdio.h>

const static int BLOCKSIZE = 4 * 1024;

#define MAX_FILENAME_LENGTH 20

typedef struct disk {
    uint32_t size;   // size of the disk
    uint32_t blocks; // number of usable blocks (except stat block)
    uint32_t reads;  // number of block reads performed
    uint32_t writes; // number of block writes performed
    FILE *data;      // File pointer to persistant data
} disk;

disk *create_disk(char *filename, int nbytes);

int read_block(disk *diskptr, int blocknr, void *block_data);

int write_block(disk *diskptr, int blocknr, void *block_data);

int free_disk(disk *diskptr);

int update_disk_stats(disk *d);

#endif