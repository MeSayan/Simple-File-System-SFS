#include "../disk.h"
#include <stdio.h>
#include <string.h>

int main() {
    disk *d1 = create_disk("../db/data", 40960);
    if (d1 == NULL) {
        printf("Failed to create disk\n");
        return 1;
    }
    printf("Disk size: %d \n", d1->size);
    printf("Disk reads: %d\n", d1->reads);
    printf("Disk writes: %d\n", d1->writes);
    printf("No of usable blocks: %d\n", d1->blocks);
    printf("Size of disk struct: %ld\n", sizeof(*d1));

    char buf[4096];
    int ret;
    char res[4096];

    memset(buf, 0, BLOCKSIZE);
    for (int b = 0; b < d1->blocks - 2; ++b) {
        sprintf(buf, "Hello world how are you %d", b);
        ret = write_block(d1, b, buf);
        if (ret == -1) {
            printf("Write failed\n");
        }
    }

    printf("Disk summary\n");
    printf("Disk size: %d \n", d1->size);
    printf("No of usable blocks: %d\n", d1->blocks);
    printf("Disk reads: %d\n", d1->reads);
    printf("Disk writes: %d\n", d1->writes);

    for (int b = 0; b < d1->blocks; ++b) {
        memset(res, 0, BLOCKSIZE);
        ret = read_block(d1, b, res);
        if (ret == -1) {
            printf("Read failed\n");
        }
        printf("%s\n", res);
    }

    printf("Disk summary\n");
    printf("Disk size: %d \n", d1->size);
    printf("No of usable blocks: %d\n", d1->blocks);
    printf("Disk reads: %d\n", d1->reads);
    printf("Disk writes: %d\n", d1->writes);

    return 0;
}