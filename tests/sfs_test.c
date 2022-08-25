#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "../disk.h"
#include "../sfs.h"

void check_all_inodes(disk *diskptr, super_block *s) {
    // Check all inodes
    char buf[BLOCKSIZE];
    for (int b = s->inode_block_idx; b < s->data_block_idx; ++b) {
        read_block(diskptr, b, buf);
        for (int i = 0; i < BLOCKSIZE / sizeof(inode); ++i) {
            inode *in = (inode *)(buf + i * sizeof(inode));
            printf("inode val: %d\n", in->valid);
        }
    }
}

void file_test() {
    // Create 10 files
    // delete 9 th file
    // 11 th file created should have inode 9
    int files[10];
    for (int i = 0; i < 10; ++i)
        files[i] = create_file();

    printf("Create all files\n");
    for (int i = 0; i < 10; ++i)
        printf("%d ", files[i]);
    printf("\n");

    for (int i = 0; i < 10; ++i) {
        stat(files[i]);
    }

    remove_file(files[8]);
    stat(files[8]);
}

void print_super_block(disk *diskptr, super_block *s) {
    printf("Magic Number: %d\n", s->magic_number);
    printf("# Blocks: %d\n", s->blocks);
    printf("# Inode Blocks %d\n", s->inode_blocks);
    printf("# Inodes: %d\n", s->inodes);
    printf("# Data Blocks: %d\n", s->data_blocks);
    printf("Inode Bitmap First Block:  %d\n", s->inode_bitmap_block_idx);
    printf("Inode Block First Block:  %d\n", s->inode_block_idx);
    printf("Data Block Bitmap First Block:  %d\n", s->data_block_bitmap_idx);
    printf("Data Block First Block: %d\n", s->data_block_idx);
}

void check_initial_inodes(disk *diskptr) {
    char sb[BLOCKSIZE];
    super_block s;
    read_block(diskptr, 0, (void *)sb);
    s = *(super_block *)sb;
    print_super_block(diskptr, &s);
}

char *create_long_string(int size) {
    srand(time(NULL));
    char *st = (char *)malloc(sizeof(char) * size);
    for (int i = 0; i < size - 1; ++i) {
        /* Assign random characters */
        st[i] = 65 + rand() % 26;
    }
    st[size - 1] = '\0';
    return st;
}
void read_write_test(int inumber) {
    char temp[100];
    sprintf(temp, "Hello File system");
    write_i(inumber, temp, 100, 0);
    stat(inumber);

    char temp2[100];
    read_i(inumber, temp2, 100, 0);
    printf("%s\n", temp2);
    stat(inumber);

    // truncate to 200, should have no effect
    fit_to_size(inumber, 200);
    stat(inumber);
    memset(temp2, 0, 100);
    read_i(inumber, temp2, 100, 0);
    printf("%s\n", temp2);

    // truncate to 5, should have effect
    fit_to_size(inumber, 5);
    printf("After truncating to 5\n");
    stat(inumber);
    memset(temp2, 0, 100);
    read_i(inumber, temp2, 100, 0);
    printf("%s\n", temp2);

    remove_file(inumber);
    stat(inumber);
}

void long_read_write_test(int inumber) {
    int length = 50000;
    char *s = create_long_string(length);
    int i = write_i(inumber, s, length, 0);
    printf("\nAfter Writing (%d bytes read)\n", i);
    stat(inumber);

    char y[length];
    int j = read_i(inumber, y, length, 0);
    printf("\nAfter Reading (%d bytes read)\n", j);
    // printf("%.*s\n", length, y);

    int result = strncmp(s, y, length);
    printf("Result of string comparision: %c\n", result == 0 ? 'T' : 'F');
}

int main() {

    disk *d1 = create_disk("../db/data", 409600);
    format(d1);
    // check_initial_inodes(d1);
    mount(d1, MRD_Y);
    // file_test();
    int f1 = create_file();
    // read_write_test(f1);

    long_read_write_test(f1);

    int f2 = create_file();
    long_read_write_test(f2);

    remove_file(f2);
    stat(f2);
    stat(f1);
    return 0;
}