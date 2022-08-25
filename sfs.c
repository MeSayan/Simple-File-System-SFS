#include <math.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include <time.h>
#include <stdlib.h>

#include "disk.h"
#include "sfs.h"

/* the mounted disk storage */
disk *mounted_diskptr = NULL;

/* invalid (out of range) block pointer*/
#define INVALID UINT32_MAX

/* Print Inode summary */
void print_inode(int inumber, inode *i) {
    printf("Inode Summary (%d): \n", inumber);
    printf("Valid: %d \n", i->valid);
    printf("Size: %d \n", i->size);
    printf("Direct Pointers: %d %d %d %d %d \n", i->direct[0], i->direct[1],
           i->direct[2], i->direct[3], i->direct[4]);
    printf("Indirect Pointer: %d \n\n", i->indirect);
}

/* Read the superblock. Return -1 on error 0 on no error */
int get_super_block(disk *diskptr, super_block *s) {
    int ret = -1;
    char buf[BLOCKSIZE];

    ret = read_block(diskptr, 0, (void *)buf);
    if (ret == -1) return -1;

    *s = *(super_block *)buf;
    return 0;
}

/* Read inode structure for inumber inode */
int get_inode(disk *diskptr, int inumber, inode *in) {
    super_block s;
    int ret;

    ret = get_super_block(diskptr, &s);
    if (ret == -1) return -1;

    /*Check if valid file */
    if (inumber > s.inodes || inumber < 0) return -1;

    int block_offset = inumber / (BLOCKSIZE / sizeof(inode));
    int block_offset_index = inumber % (BLOCKSIZE / sizeof(inode));
    char buf[BLOCKSIZE];
    ret = read_block(diskptr, s.inode_block_idx + block_offset, (void *)buf);
    if (ret == -1) {
        return -1;
    }

    *in = *(inode *)(buf + block_offset_index * sizeof(inode));
    return 0;
}

/* Writes the inode to disk */
int write_inode_to_disk(disk *diskptr, int inumber, inode *in) {
    super_block s;
    int ret;

    ret = get_super_block(diskptr, &s);
    if (ret == -1) return -1;

    int block_offset = inumber / (BLOCKSIZE / sizeof(inode));
    int block_offset_index = inumber % (BLOCKSIZE / sizeof(inode));
    char buf[BLOCKSIZE];
    ret = read_block(diskptr, s.inode_block_idx + block_offset, (void *)buf);
    if (ret == -1) {
        return -1;
    }
    /* Update */
    memcpy(buf + block_offset_index * sizeof(inode), in, sizeof(inode));

    /* Write to disk */
    ret = write_block(diskptr, s.inode_block_idx + block_offset, (void *)buf);
    return ret;
}

/* Read all data blocks for a inode file. Returns an ordered view
   of all the datablocks (from direct and indirect pointers) which
   are used by the file
*/
int get_all_data_blocks(disk *diskptr, int inumber, int *res) {
    /* res is a 5 + 1024 = 1029 element array */
    int ret;
    super_block s;
    ret = get_super_block(diskptr, &s);
    if (ret == -1) return -1;

    inode in;
    ret = get_inode(diskptr, inumber, &in);
    if (ret == -1) return -1;

    int c = 0;

    if (in.size == 0) {
        for (int i = 0; i < 1029; ++i)
            res[i] = INVALID;
        return 0;
    }

    int cumsum = 0;
    /* Read direct pointers */
    for (int i = 0; i < 5; ++i) {
        if (in.direct[i] >= 0 && in.direct[i] < s.data_blocks) {
            res[c++] = in.direct[i];
            cumsum += BLOCKSIZE;
            if (cumsum >= in.size) break;
        }
    }

    /* Read indirect pointers */
    if (in.indirect >= 0 && in.indirect < s.data_blocks && cumsum < in.size) {
        char buf[BLOCKSIZE];
        ret = read_block(diskptr, s.data_block_idx + in.indirect, (void *)buf);
        if (ret == -1) return -1;

        for (int i = 0; i < 1024; i++) {
            int x = *(int *)(buf + i * 4);
            if (x >= 0 && x < s.data_blocks) {
                res[c++] = x;
                cumsum += BLOCKSIZE;
                if (cumsum >= in.size) break;
            }
        }
    }

    for (int i = c; i < 1029; ++i)
        res[i] = INVALID;
    return 0;
}

/* Operate  on inode / data bitmap
    mode = 0 => Reset
    mode = 1 => Set
    mode = 2 => read
*/
int operate_bitmap(disk *diskptr, int bitmap_base, int bitmap_offset,
                   int mode) {
    int ret;
    int block_no = bitmap_offset / (8 * BLOCKSIZE);
    int block_offset = bitmap_offset % (8 * BLOCKSIZE);
    int block_byte_offset = block_offset / 8;
    int block_byte_bit_offset = block_offset % 8;
    char buf[BLOCKSIZE];
    /* Read */
    ret = read_block(diskptr, bitmap_base + block_no, (void *)buf);
    if (ret == -1) return -1;

    /* Update */
    if (mode == 0) {
        buf[block_byte_offset] = buf[block_byte_offset] &
                                 (~(1 << (7 - block_byte_bit_offset)));
    } else if (mode == 1) {
        buf[block_byte_offset] = buf[block_byte_offset] |
                                 ((1 << (7 - block_byte_bit_offset)));
    } else if (mode == 2) {
        return buf[block_byte_offset] & ((1 << (7 - block_byte_bit_offset)));
    }

    /* Write */
    ret = write_block(diskptr, bitmap_base + block_no, (void *)buf);
    return ret;
}

/* Finds a free bitmap, sets it and returns index */
int get_free_bitmap(disk *diskptr, int bmp_start, int bmp_end) {
    char buf[BLOCKSIZE];
    int ret;

    for (int b = bmp_start; b < bmp_end; ++b) {
        ret = read_block(diskptr, b, (void *)buf);
        if (ret == -1) return -1;
        for (int byte = 0; byte < BLOCKSIZE; ++byte) {
            for (int bit = 0; bit < 8; ++bit) {
                if (!(buf[byte] & (1 << (7 - bit)))) {
                    int index = (b - bmp_start) * 4096 + byte * 8 + bit;
                    buf[byte] = buf[byte] | (1 << (7 - bit));
                    ret = write_block(diskptr, b, (void *)buf);
                    if (ret == -1) return -1;
                    return index;
                }
            }
        }
    }

    /* end of disk - no inode available */
    return -2;
}

/* Prints no of inodes and data blocks used */
void show_stats() {
    int ret;
    super_block s;
    ret = get_super_block(mounted_diskptr, &s);
    if (ret == -1) return;

    int consumed_db = 0;
    for (int i = 0; i < s.data_blocks; ++i) {
        if (operate_bitmap(mounted_diskptr, s.data_block_bitmap_idx, i, 2)) {
            consumed_db++;
        }
    }

    int consumed_in = 0;
    for (int i = 0; i < s.inode_blocks; ++i) {
        if (operate_bitmap(mounted_diskptr, s.inode_bitmap_block_idx, i, 2)) {
            consumed_in++;
        }
    }

    printf("\n     Filesystem Statistics:    \n");
    printf("=================================\n");
    printf("Used Inodes : %d / %d\n", consumed_in, s.inodes);
    printf("Used Data Blocks: %d / %d\n", consumed_db, s.data_blocks);
    printf("\n        Disk Statistics:       \n");
    printf("=================================\n");
    printf("# Blocks: %d\n", mounted_diskptr->blocks);
    printf("# Bytes %d\n", mounted_diskptr->size);
    printf("# Reads: %d\n", mounted_diskptr->reads);
    printf("# Writes: %d\n\n", mounted_diskptr->writes);
}

/* Returns minimum of x, y*/
int get_min(int x, int y) {
    if (x <= y) return x;
    return y;
}

/* Returns maximum of x, y*/
int get_max(int x, int y) {
    if (x >= y) return x;
    return y;
}

int create_root_directory();

/* Formats the file system properly setting up superblock, bitmaps and
inodes. Return -1 on error and 0 on success
*/
int format(disk *diskptr) {
    int ret = -1;

    /* one block reserved for superblock */
    int M = diskptr->blocks - 1;
    /* no of inode blocks */
    int I = (int)floor(0.1 * M);
    /* no of inodes */
    int nInodes = I * 128;
    /* no of blocks reserved for inode bitmap */
    int IB = (int)ceil(1.0 * nInodes / (8 * 4096));
    /* no of data blocks + data blocks bitmap */
    int R = M - I - IB;
    /* no of data blocks bitmap */
    int DBB = (int)ceil(1.0 * R / (8 * 4096));
    /* no of data blocks */
    int DB = R - DBB;

    super_block s;
    s.magic_number = MAGIC;
    s.blocks = M;
    s.inode_blocks = I;
    s.inodes = nInodes;
    s.inode_bitmap_block_idx = 1;
    s.inode_block_idx = 1 + IB + DBB;
    s.data_block_bitmap_idx = 1 + IB;
    s.data_block_idx = 1 + IB + DBB + I;
    s.data_blocks = DB;

    /* Write superblock to disk */
    write_block(diskptr, 0, (void *)&s);

    /* initialize bitmaps
       empty block */
    char eb[BLOCKSIZE];
    memset(eb, 0, BLOCKSIZE);

    /* initialize inode bitmaps */
    for (int b = s.inode_bitmap_block_idx; b < s.data_block_bitmap_idx; ++b) {
        ret = write_block(diskptr, b, (void *)eb);
        if (ret == -1) return -1;
    }

    /* initialize data bitmaps */
    for (int b = s.data_block_bitmap_idx; b < s.inode_block_idx; ++b) {
        ret = write_block(diskptr, b, (void *)eb);
        if (ret == -1) return -1;
    }

    /* initialize inodes */
    for (int b = s.inode_block_idx; b < s.data_block_idx; ++b) {
        char ib[BLOCKSIZE];
        memset(ib, 0, BLOCKSIZE);
        for (int i = 0; i < BLOCKSIZE / sizeof(inode); ++i) {
            inode in;
            in.valid = 0;
            memcpy(ib + (i * sizeof(inode)), &in, sizeof(inode));
        }
        /* put block in disk */
        ret = write_block(diskptr, b, (void *)ib);
        if (ret == -1) return -1;
    }

    return 0;
}

/* Mounts the filesystem for use */
int mount(disk *diskptr, int mount_root_directory_flg) {
    /* Read superblock (first block and check magic number */
    super_block s;
    int ret = get_super_block(diskptr, &s);
    if (ret == -1) {
        return -1;
    }

    if (s.magic_number != MAGIC) {
        return -1;
    }

    mounted_diskptr = diskptr;

    if (mount_root_directory_flg) {
        /* Create root directory and make fs ready for read/write files
           create_root_directory() is implemented later in this file, along
           with other Part C functions.
        */
        ret = create_root_directory();
        return ret;
    }

    return 0;
}

/* Creates the file and returns its inode. On error returns -1 */
int create_file() {
    /* Check if filesystem is mounted */
    if (mounted_diskptr == NULL) return -1;

    super_block s;
    int ret;

    ret = get_super_block(mounted_diskptr, &s);
    if (ret == -1) return -1;

    /* Scan through inode bitmap to find empty inode (and set it) */
    int inode_index = get_free_bitmap(mounted_diskptr, s.inode_bitmap_block_idx,
                                      s.data_block_bitmap_idx);

    /* disk full */
    if (inode_index == -2) return -1;

    inode in;
    ret = get_inode(mounted_diskptr, inode_index, &in);
    if (ret == -1) return -1;

    /* Initialize file */
    in.size = 0;
    in.valid = 1;
    in.indirect = INVALID;
    in.direct[0] = in.direct[1] = INVALID;
    in.direct[2] = in.direct[3] = INVALID;
    in.direct[4] = INVALID;

    /* write inode */
    int block_offset = inode_index / (BLOCKSIZE / sizeof(inode));
    int block_offset_index = inode_index % (BLOCKSIZE / sizeof(inode));
    char buf[BLOCKSIZE];
    ret = read_block(mounted_diskptr, s.inode_block_idx + block_offset,
                     (void *)buf);
    memcpy(buf + block_offset_index * sizeof(inode), &in, sizeof(inode));

    ret = write_block(mounted_diskptr, s.inode_block_idx + block_offset,
                      (void *)buf);

    return inode_index;
}

/* Removes the file freeing up inodes and bitmaps.
 Returns 0 on success and -1 on error
*/
int remove_file(int inumber) {
    /* Check if filesystem is mounted */
    if (mounted_diskptr == NULL) return -1;

    /* To remove file
        1. Set inode valid to 0
        2. Reset inode and data bitmaps
    */

    /* Get superblock and the file inode */
    int ret;
    super_block s;
    ret = get_super_block(mounted_diskptr, &s);
    if (ret == -1) return -1;

    inode in;
    ret = get_inode(mounted_diskptr, inumber, &in);
    if (ret == -1) return -1;

    in.valid = 0;

    /* update inode bitmap */
    /* free Bitmap */
    ret = operate_bitmap(mounted_diskptr, s.inode_bitmap_block_idx, inumber, 0);
    if (ret == -1) return -1;

    /* update data bitmap */
    uint32_t res[1029];
    ret = get_all_data_blocks(mounted_diskptr, inumber, res);
    if (ret == -1) return -1;

    for (int i = 0; i < 1029; ++i) {
        if (res[i] >= 0 && res[i] < s.data_blocks) {
            /* Free Bitmap */
            ret = operate_bitmap(mounted_diskptr, s.data_block_bitmap_idx,
                                 res[i], 0);
        }
    }

    /* Free indirect pointer */
    if (in.indirect >= 0 && in.indirect < s.data_blocks) {
        ret = operate_bitmap(mounted_diskptr, s.data_block_bitmap_idx,
                             in.indirect, 0);
    }

    /* Write inode to disk */
    return write_inode_to_disk(mounted_diskptr, inumber, &in);
}

/* Outputs the stats of the inode */
int stat(int inumber) {
    /* Check if filesystem is mounted */
    if (mounted_diskptr == NULL) return -1;

    /* Get superblock and the file inode */
    int ret;
    super_block s;
    ret = get_super_block(mounted_diskptr, &s);
    if (ret == -1) return -1;

    inode in;
    ret = get_inode(mounted_diskptr, inumber, &in);

    if (ret == -1) {
        return -1;
    }

    printf("\nInode (%d) Statistics: \n", inumber);
    printf("=======================\n");
    printf("Valid Bit: %d\n", in.valid);

    uint32_t res[1029];
    ret = get_all_data_blocks(mounted_diskptr, inumber, res);

    if (ret == -1) return -1;

    int c = 0;
    for (int i = 0; i < 1029; ++i) {
        if (res[i] >= 0 && res[i] < s.data_blocks) c++;
    }

    if (in.valid) {
        printf("Size: %d\n", in.size);
        printf("No of blocks in use: %d\n", c);
        printf("No of direct pointers in use: %d\n", c > 5 ? 5 : c);
        printf("No of indirect pointers in use: %d\n\n", c > 5 ? c - 5 : 0);
    } else {
        /* invalid inodes dont have any set data bitmaps
           not using any space
        */
        printf("Size: %d\n", 0);
        printf("No of blocks in use: %d\n", 0);
        printf("No of direct pointers in use: %d\n", 0);
        printf("No of indirect pointers in use: %d\n\n", 0);
    }

    return 0;
}

/* Starting from offset position in file, read length bytes form file to data
 * buffer file. Return -1 on error and other wise returns no of bytes read
 */
int read_i(int inumber, char *data, int length, int offset) {
    /* Check if filesystem is mounted */
    if (mounted_diskptr == NULL) return -1;

    /* Get superblock and the file inode */
    int ret;
    super_block s;
    ret = get_super_block(mounted_diskptr, &s);
    if (ret == -1) return -1;

    inode in;
    ret = get_inode(mounted_diskptr, inumber, &in);

    /* Validation */
    if (ret == -1 || in.valid == 0 || offset < 0 || offset > in.size ||
        length < 0)
        return -1;

    int bytes_to_read = 0;
    if (length >= (in.size - offset))
        bytes_to_read = (in.size - offset);
    else
        bytes_to_read = length;

    uint32_t res[1029];
    ret = get_all_data_blocks(mounted_diskptr, inumber, res);
    int st = offset, c = 0, r = 0;
    char buf[BLOCKSIZE];
    while (bytes_to_read > 0) {
        int i = st / BLOCKSIZE;
        int j = st % BLOCKSIZE;
        int k = BLOCKSIZE - j;
        int r = get_min(bytes_to_read, k);
        ret = read_block(mounted_diskptr, s.data_block_idx + res[i],
                         (void *)buf);
        if (ret == -1) return -1;
        memcpy(data + c, buf + j, r);

        bytes_to_read -= r;
        st += r;
        c += r;
    }

    return length - bytes_to_read; // no of bytes read
}

/* Starting from offset position in file, write length bytes form data to the
 * file. Return -1 on error and other wise returns no of bytes written
 */
int write_i(int inumber, char *data, int length, int offset) {
    /* Check if filesystem is mounted */
    if (mounted_diskptr == NULL) return -1;

    /* Nothing to write if length is 0 */
    if (length == 0) return 0;

    /* Get superblock and the file inode */
    int ret;
    super_block s;
    ret = get_super_block(mounted_diskptr, &s);
    if (ret == -1) return -1;

    inode in;
    ret = get_inode(mounted_diskptr, inumber, &in);
    if (ret == -1) return -1;

    /* Validation */
    if (in.valid == 0 || offset > in.size) return -1;

    uint32_t res[1029], c = 0;
    ret = get_all_data_blocks(mounted_diskptr, inumber, res);
    int index = offset / BLOCKSIZE;
    int index_off = offset % BLOCKSIZE;

    int bytes_to_write = length;
    char buf[BLOCKSIZE];

    while (bytes_to_write > 0) {
        if (res[index] == INVALID) {
            /* Empty block so allocate data block */
            int db_index = get_free_bitmap(
                mounted_diskptr, s.data_block_bitmap_idx, s.inode_block_idx);
            if (db_index == -2) return length - bytes_to_write; // disk full
            if (db_index == -1) return -1;
            res[index] = db_index;
        }

        ret = read_block(mounted_diskptr, s.data_block_idx + res[index],
                         (void *)buf);
        int k = get_min(bytes_to_write, BLOCKSIZE - index_off);

        memcpy(buf + index_off, data + c, k);
        c += k;

        ret = write_block(mounted_diskptr, s.data_block_idx + res[index],
                          (void *)buf);
        if (ret == -1) return -1;
        bytes_to_write -= k;

        index += 1;
        index_off = 0;
    }

    /* Fit modified res back to inode */
    memset(buf, 0, BLOCKSIZE);
    int wr = 0;
    for (int i = 0; i < 1029; ++i) {
        if (i < 5) {
            in.direct[i] = res[i];
        } else if (i >= 5 && (res[i] >= 0 && res[i] < s.data_blocks)) {
            memcpy(buf + (i - 5) * sizeof(int), (char *)&(res[i]), sizeof(int));
            wr += 1;
        }
    }

    if (wr > 0) {
        if (in.indirect >= 0 && in.indirect < s.data_blocks) {
            ret = write_block(mounted_diskptr, s.data_block_idx + in.indirect,
                              (void *)buf);
            if (ret == -1) return -1;
        } else {
            in.indirect = get_free_bitmap(
                mounted_diskptr, s.data_block_bitmap_idx, s.inode_block_idx);
            if (in.indirect >= 0) {
                ret = write_block(mounted_diskptr,
                                  s.data_block_idx + in.indirect, (void *)buf);
                if (ret == -1) return -1;
            }
        }
    } else {
        in.indirect = INVALID;
    }

    /* Update size and write inode to disk */
    in.size = get_max(offset + length, in.size);
    ret = write_inode_to_disk(mounted_diskptr, inumber, &in);
    if (ret == -1) return -1;

    return length - bytes_to_write; // no of bytes written
}

/* Truncates the file to specified size.
   Returns 0 on success and -1 on error
*/
int fit_to_size(int inumber, int size) {
    /* Check if filesystem is mounted */
    if (mounted_diskptr == NULL) return -1;

    /* Get superblock and inode */
    int ret;
    inode in;
    super_block s;

    ret = get_super_block(mounted_diskptr, &s);
    if (ret == -1) return -1;

    ret = get_inode(mounted_diskptr, inumber, &in);
    if (ret == -1) return -1;

    if (in.size > size) {
        /* no of blocks to keep. remove any blocks in excess of this */
        int nblocks = (int)ceil(1.0 * size / BLOCKSIZE);

        /* Removes blocks after nblocks */
        if (nblocks <= 5) {
            for (int b = nblocks + 1; b <= 5; ++b) {
                int db_array_index = b - 1;
                operate_bitmap(mounted_diskptr, s.data_block_bitmap_idx,
                               in.direct[db_array_index], 0);
            }
        }

        /* no of indirect blocks to keep*/
        int rem = nblocks - 5;

        double buf[BLOCKSIZE];
        memset(buf, -1, BLOCKSIZE);

        if (in.indirect >= 0 && in.indirect < s.data_blocks) {
            double blk[BLOCKSIZE];
            int ret = read_block(mounted_diskptr,
                                 s.data_block_idx + in.indirect, (void *)blk);
            if (ret == -1) return -1;

            for (int i = 0; i < 1024; ++i) {
                if (i >= rem) {
                    int x = *(int *)(blk + i * 4);
                    if (x >= 0 && x < s.data_blocks)
                        operate_bitmap(mounted_diskptr, s.data_block_bitmap_idx,
                                       x, 0);
                }
            }
            memcpy(buf, blk, rem * 4);
            ret = write_block(mounted_diskptr, s.data_block_idx + in.indirect,
                              (void *)buf);
            if (ret == -1) return -1;
        }

        in.size = size;
        /* Update inode on disk */
        ret = write_inode_to_disk(mounted_diskptr, inumber, &in);
        if (ret == -1) return -1;
    }
    return 0;
}

/* Initialises to an invalid inode */
void initialise_inode(inode *in) {
    in->valid = 1;
    in->size = 0;
    in->direct[0] = in->direct[1] = in->direct[2] = INVALID;
    in->direct[3] = in->direct[4] = INVALID;
    in->indirect = INVALID;
}

/* Returns the entire contents of the file pointed by inode
   in a string
*/
char *read_file_contents(int inumber, int *content_size) {
    int ret;
    inode in;
    ret = get_inode(mounted_diskptr, inumber, &in);

    char *content = (char *)malloc(sizeof(char) * in.size);
    ret = read_i(inumber, content, in.size, 0);
    if (ret == -1) return NULL;

    /* set content size */
    *content_size = ret;
    return content;
}

/* Parses the content of a directory file, and returns
   the list of children (files/sub-directories) of the directory
*/
child *parse_content(int filesize, char *content) {
    int nitems = (int)(1.0 * filesize / sizeof(child));
    child *items = (child *)malloc(sizeof(child) * nitems);
    for (int i = 0; i < nitems; ++i) {
        items[i] = *(child *)(content + i * sizeof(child));
    }
    return items;
}

/* Returns the walk from root to the item and stores length
   in the parameter passed.
*/
char **get_walk_from_root(char *path, int *length) {
    /*Max string length of walk is MAX_FILENAME * no of inodes*/
    int ret;
    super_block s;
    ret = get_super_block(mounted_diskptr, &s);
    int max_path_length = MAX_FILENAME * s.inodes;

    char path_cp[max_path_length];
    strcpy(path_cp, path);

    char **token = (char **)(malloc(sizeof(char *) * s.inodes));
    for (int i = 0; i < s.inodes; ++i) {
        token[i] = (char *)malloc(sizeof(char) * MAX_FILENAME);
    }

    char *tok = strtok(path_cp, "/");
    int c = 0;
    while (tok != NULL) {
        strcpy(token[c++], tok);
        tok = strtok(NULL, "/");
    }
    *length = c;
    return token;
}

/* Converts a file/directory path to the corresponding inode */
int name_to_inode(char *path, int type) {
    int c = 0;
    char **tok = get_walk_from_root(path, &c);

    int inode_id = 0, level = 0;
    int ret;
    inode in;

    while (level < c) {
        int content_size;
        char *content = read_file_contents(inode_id, &content_size);

        ret = get_inode(mounted_diskptr, inode_id, &in);
        if (ret == -1) return INVALID;

        child *items = parse_content(in.size, content);
        int nitems = (int)(1.0 * in.size / sizeof(child));
        int found = 0;

        /* Check for file or directory depending on level
        in last level need to find file and in other levels
        need to find the intermediate directories
        */
        int check_type = type;
        if (type == SFS_TYPE_F) {
            check_type = (level < c - 1) ? SFS_TYPE_D : SFS_TYPE_F;
        }

        /* Crawl through items at each level */
        for (int i = 0; i < nitems; ++i) {
            if (strcmp(items[i].name, tok[level]) == 0 &&
                items[i].type == check_type) {
                /* Found directory */
                inode_id = items[i].inumber;
                level++;
                found = 1;
                break;
            }
        }

        free(content);
        free(items);

        if (found == 0) {
            /* intermediate directory not found */
            inode_id = INVALID;
            break;
        }
    }

    /* Cleanup */
    for (int i = 0; i < c; ++i)
        free(tok[i]);

    /* After the walk, inode_id is the final inode */
    return inode_id;
}

/* Extracts and returns the parent path of the input path */
char *get_parent_path(char *child_path) {
    char *p = strrchr(child_path, '/');
    char *parent_path = (char *)malloc(sizeof(char) * (p - child_path + 2));
    strncpy(parent_path, child_path, p - child_path + 1);
    if (p == child_path) {
        parent_path[p - child_path + 1] = '\0';
    } else {
        parent_path[p - child_path] = '\0';
    }
    return parent_path;
}

/* Adds an empty file to parent directory.
   Returns file inode no if successful and -1 for other errors.
   If file already exists then also returns -1
*/
int add_file_to_directory(char *filepath) {
    int ret;
    uint32_t inumber = name_to_inode(filepath, SFS_TYPE_F);
    if (inumber == INVALID) {
        /* File does not exist */
        char *parent_path = get_parent_path(filepath);
        uint32_t parent_inode_no = name_to_inode(parent_path, SFS_TYPE_D);
        if (parent_inode_no == INVALID) return -1;

        /*Parent exists - Create file and update parent*/
        ret = create_file();
        if (ret == -1) return -1;
        inumber = ret;

        /* Update parent */
        child entry;
        entry.inumber = inumber;
        entry.type = SFS_TYPE_F;
        entry.valid = 1;
        char *chldname = strstr(filepath, parent_path) + strlen(parent_path) +
                         1;
        strncpy(entry.name, chldname, MAX_FILENAME);
        entry.length = strlen(chldname);
        inode parent_inode;

        ret = get_inode(mounted_diskptr, parent_inode_no, &parent_inode);
        if (ret == -1) return -1;

        ret = write_i(parent_inode_no, (char *)&entry, sizeof(entry),
                      parent_inode.size);
        if (ret == -1) return ret;

        /* All ok return inode of newly added file */
        return inumber;
    }
    /* File already exists */
    return -1;
}

/*Removes an item/file from the directory listing*/
int remove_item_from_directory_file(int inumber, char *name, int type) {
    int ret;
    inode current;
    ret = get_inode(mounted_diskptr, inumber, &current);
    if (ret == -1) return -1;

    int content_size;
    char *content = read_file_contents(inumber, &content_size);
    int sz = current.size;
    int nitems = sz / sizeof(child);

    char modified_content[sz];
    int c = 0;

    for (int i = 0; i < nitems; ++i) {
        child e = *(child *)(content + i * sizeof(child));
        if ((strcmp(e.name, name) == 0) && e.type == type) {
            /* Skip writing to modified content to delete */
            continue;
        }
        memcpy(modified_content + c, &e, sizeof(child));
        c += sizeof(child);
    }

    /* Update parent directory file */
    fit_to_size(inumber, 0);
    ret = write_i(inumber, modified_content, c, 0);
    if (ret == -1) return -1;

    /* All ok */
    return 0;
}

/*  Function to create root directory. Sets inode 0 for the / directory
    Returns 0 on success and -1 on error
*/
int create_root_directory() {
    /* Root directory is always assigned inode 0 */

    /* Fetch superblock and inode 0 */
    int ret;
    super_block s;
    inode in;

    ret = get_super_block(mounted_diskptr, &s);
    ret = get_inode(mounted_diskptr, 0, &in);

    /*Delete existing file*/
    if (in.valid) {
        remove_file(0);
    }

    operate_bitmap(mounted_diskptr, s.inode_bitmap_block_idx, 0, 1);
    initialise_inode(&in);
    ret = write_inode_to_disk(mounted_diskptr, 0, &in);

    if (ret == -1) return -1;

    /* All ok */
    return 0;
}

/*  Read the file - length bytes starting from offset.
    Returns no of bytes read from file
*/
int read_file(char *filepath, char *data, int length, int offset) {
    /* Check if filesystem is mounted */
    if (mounted_diskptr == NULL) return -1;

    uint32_t inumber = name_to_inode(filepath, SFS_TYPE_F);
    if (inumber == INVALID) return -1;
    int ret = read_i(inumber, data, length, offset);
    return ret;
};

/* Writes length bytes from data to the file specified (offset position onwards)
   If file is not present, the file is created.
   Return no of bytes writtent to file
*/
int write_file(char *filepath, char *data, int length, int offset) {
    /* Check if filesystem is mounted */
    if (mounted_diskptr == NULL) return -1;

    int ret;
    uint32_t inumber = name_to_inode(filepath, SFS_TYPE_F);
    if (inumber == INVALID) {
        /* File does not exists, create an emtpy file*/
        ret = add_file_to_directory(filepath);
        if (ret == -1) return -1;
        inumber = ret;
    }
    ret = write_i(inumber, data, length, offset);
    return ret;
};

/*  Creates the directory (other than root directory).
    To create root directory internal function create_root_directory() (which
    is called after mounting) should be used.
    Only adds the directory if the immediate parent directory is present.
    Return -1 on errors, otherwise returns inode  number of directory
    created
*/
int create_dir(char *dirpath) {
    /* Check if filesystem is mounted */
    if (mounted_diskptr == NULL) return -1;

    int c, ret;
    /* Update parent directory and create empty file */
    char *parent_path = get_parent_path(dirpath);

    if (strcmp(dirpath, "/") == 0) {
        /* This function should not be used to create root directory,
           create_root_directory() function should be used, which is called
           in mount() funtion.
        */
        return -1;
    } else {
        int parent_inode_no = name_to_inode(parent_path, SFS_TYPE_D);
        if (parent_inode_no == INVALID) {
            /* Didnot find parent directory */
            free(parent_path);
            return -1;
        }
        int child_inode_no = create_file();
        /* Add directory entry to the file */
        child entry;
        entry.inumber = child_inode_no;
        entry.type = SFS_TYPE_D;
        char *chldname = strrchr(dirpath, '/') + 1;
        strncpy(entry.name, chldname, MAX_FILENAME);
        entry.valid = 1;
        entry.length = strlen(chldname);

        inode parent_inode;
        ret = get_inode(mounted_diskptr, parent_inode_no, &parent_inode);

        if (ret != -1)
            ret = write_i(parent_inode_no, (char *)&entry, sizeof(entry),
                          parent_inode.size);

        free(parent_path);
        if (ret == -1)
            return INVALID;
        else
            return child_inode_no;
    }

    return ret;
}

/* Removes directory and recursively deleting all sub-directories and files.
   Hence this function removes the complete subtree rooted at dirpath
   Returns 0 on success and -1 otherwise
*/
int remove_dir(char *dirpath) {

    /* Check if filesystem is mounted */
    if (mounted_diskptr == NULL) return -1;

    int ret;
    inode in;
    super_block s;

    /* Get dirpath inode */
    uint32_t inode_no = name_to_inode(dirpath, SFS_TYPE_D);
    if (inode_no == INVALID) return -1;

    /*Get superblock */

    ret = get_super_block(mounted_diskptr, &s);
    if (ret == -1) return -1;

    /* Get dirpath inode */
    ret = get_inode(mounted_diskptr, inode_no, &in);
    if (ret == -1) return -1;

    /* Update Parent (if not root itself) */
    if (strcmp(dirpath, "/") != 0) {
        char *parent_path = get_parent_path(dirpath);
        char *child_name = strrchr(dirpath, '/') + 1;

        uint32_t parent_inode_no = name_to_inode(parent_path, SFS_TYPE_D);
        ret = remove_item_from_directory_file(parent_inode_no, child_name,
                                              SFS_TYPE_D);
        if (ret == -1) return -1;
    }

    /* Bread First Deletion */

    /* Inode Queue */
    uint32_t Q[s.inodes + 1];
    int left = 0, right = 1;
    Q[0] = inode_no;

    while (left < right) {
        uint32_t head = Q[left++];
        /* Arrived at head */
        int content_size;
        char *content = read_file_contents(head, &content_size);
        if (content != NULL) {
            child entry;
            for (int i = 0; i < content_size / sizeof(child); i++) {
                entry = *(child *)(content + i * sizeof(child));
                if (entry.type == SFS_TYPE_F && entry.valid == 1) {
                    /* Delete file */
                    remove_file(entry.inumber);
                } else if (entry.type == SFS_TYPE_D && entry.valid == 1) {
                    /* Mark sub-directory for deletion */
                    Q[right++] = entry.inumber;
                }
            }
        }
        /* Delete current directory */
        remove_file(head);
    }

    return 0;
}