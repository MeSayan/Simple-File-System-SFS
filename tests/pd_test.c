#include "../disk.h"
#include <stdio.h>

int main() {
    disk *d = create_disk("data", 40960);

    printf("# Blocks: %d \n", d->blocks);
    printf("# Size: %d \n", d->size);
    printf("# Reads: %d \n", d->reads);
    printf("# Writes: %d \n", d->writes);
    printf("# Data: %p\n", d->data);

    return 0;
}