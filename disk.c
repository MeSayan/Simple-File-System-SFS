#include "disk.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <error.h>

typedef uint8_t byte;

int initialize_disk(disk *d) {
    FILE *fp = d->data;

    // Write first block (disk struct)
    int ret;
    fseek(fp, 0, SEEK_SET);
    ret = fwrite(d, sizeof(disk), 1, fp);

    // Initialize all remainining nblocks to 0
    char buf[BLOCKSIZE];
    memset(buf, 0, BLOCKSIZE);
    for (int b = 0; b < d->blocks; ++b) {
        int current = ftell(fp);
        fseek(fp, sizeof(disk) + b * BLOCKSIZE - current, SEEK_CUR);
        ret = fwrite(buf, sizeof(char), BLOCKSIZE, fp);
        if (ret != BLOCKSIZE) {
            printf("Failed to initialize disk\n");
            return -1;
        }
    }
    return 0;
}

/*If @filename exists reads it, else creates file of @nbytes size */
disk *create_disk(char *filename, int nbytes) {
    FILE *fp = fopen(filename, "r+");
    disk *d = (disk *)malloc(sizeof(disk));
    if (fp) {
        /* File exists, read struct from block */
        fseek(fp, 0, SEEK_SET);
        fread(d, sizeof(disk), 1, fp);
        /* Update file pointer */
        d->data = fp;
    } else {
        /* File doesnt exists create new file */
        fp = fopen(filename, "w+");
        if (feof(fp)) return NULL;
        d->data = fp;
        d->size = nbytes;
        d->reads = 0;
        d->writes = 0;
        d->blocks = (nbytes - sizeof(disk)) / BLOCKSIZE;
        int ret = initialize_disk(d);
        if (ret == -1) return NULL;
    }

    return d;
};

int read_block(disk *diskptr, int blocknr, void *block_data) {
    if (blocknr >= 0 && blocknr < diskptr->blocks) {
        FILE *fp = diskptr->data;
        int current = ftell(fp);
        int dest = sizeof(disk) + blocknr * BLOCKSIZE;
        int ret;
        if (current != dest) {
            ret = fseek(fp, dest - current, SEEK_CUR);
            if (ret == -1) return -1;
        }
        ret = fread(block_data, sizeof(byte), BLOCKSIZE, fp);
        if (ferror(fp)) return -1; // Any File IO error
        if (ret != BLOCKSIZE) return -1;

        /* All ok */
        diskptr->reads++;
        return 0;
    }
    return -1;
}

int write_block(disk *diskptr, int blocknr, void *block_data) {
    if (blocknr >= 0 && blocknr < diskptr->blocks) {
        FILE *fp = diskptr->data;
        int current = ftell(fp);
        int src = sizeof(disk) + blocknr * BLOCKSIZE;
        int ret;
        if (current != src) {
            ret = fseek(fp, src - current, SEEK_CUR);
            if (ret == -1) return -1;
        }
        ret = fwrite(block_data, sizeof(byte), BLOCKSIZE, fp);
        if (ferror(fp)) return -1; // Any FILE IO error
        if (ret != BLOCKSIZE) return -1;

        /* All ok */
        diskptr->writes++;
        fflush(fp);
        return 0;
    }
    return -1;
}

int free_disk(disk *diskptr) {
    free(diskptr);
    /* delete file */
};

/* Write update disk statistics to file */
int update_disk_stats(disk *d) {
    FILE *fp = d->data;
    fseek(fp, 0, SEEK_SET);
    int ret = fwrite(d, sizeof(disk), 1, fp);
    if (ferror(fp)) return -1;
    if (ret != 1) return -1;

    fflush(fp);
    return 0;
}
