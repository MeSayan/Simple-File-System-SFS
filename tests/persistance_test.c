#include "../disk.h"
#include "../sfs.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int write_file_to_FS(disk *d, char *filename_local) {
    int size;
    FILE *fp = fopen(filename_local, "rb");
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    char data[size];
    fseek(fp, 0, SEEK_SET);
    fread(data, sizeof(char), size, fp);
    create_dir("/home");
    int ret = write_file("/home/turing.c", data, size, 0);
    printf("Wrote file (%d) bytes\n", ret);
    update_disk_stats(d);
}

int read_file_from_FS(disk *d) {
    char buf[987];
    int ret = read_file("/home/turing.c", buf, 987, 0);
    printf("Read (%d) bytes\n\n", ret);
    for (int i = 0; i < ret; ++i)
        printf("%c", *(char *)(buf + i));
    update_disk_stats(d);
}

int main() {
    disk *d = create_disk("../db/data", 409600);
    // format(d);
    // mount(d, MRD_Y);
    // write_file_to_FS(d, "./persistance_test.c");

    mount(d, MRD_N);
    int x = 1;
    read_file_from_FS(d);
}
