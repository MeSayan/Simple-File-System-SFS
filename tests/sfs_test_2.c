#include <math.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include <time.h>
#include <stdlib.h>

#include "../disk.h"
#include "../sfs.h"

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

int remove_directory_test() {
    disk *disk = create_disk("data", 409600);
    format(disk);
    mount(disk, MRD_Y);

    int f1 = create_dir("/");
    int f0 = create_dir("/media");
    int f2 = create_dir("/home");

    int f3 = create_dir("/home/sayan");
    int f4 = write_file("/home/sayan/abc.txt", NULL, 0, 0);

    int f5 = create_dir("/home/shyam");
    int f6 = write_file("/home/shyam/xyz.txt", NULL, 0, 0);

    show_stats();

    int x0 = remove_dir("/home");
    printf("Status %d\n", x0);
    show_stats();

    return 0;
}

int test_0() {
    disk *d1 = create_disk("data", 409600);
    format(d1);
    mount(d1);

    int f1 = create_dir("/home");
    show_stats();
    int f2 = create_dir("/home/sayan");
    show_stats();
    int f3 = create_dir("/home/sayan/AOSD");
    show_stats();

    printf("%d %d %d\n", f1, f2, f3);

    int length = 200000;
    char *x = create_long_string(length);
    char y[length];
    memset(y, 0, length);

    printf("Writing to file /home/sayan/hello.txt\n");
    int f4 = write_file("/home/sayan/hello.txt", x, strlen(x) + 1, 0);
    printf("Wrote  %d bytes\n", f4);
    printf("Wrote (showing first 50 characters):\n %.*s\n", 50, x);
    show_stats();

    printf("Reading from file /home/sayan/hello.txt\n");
    int f5 = read_file("/home/sayan/hello.txt", y, strlen(x) + 1, 0);
    printf("Read %d bytes\n", f5);
    printf("Read (showing first 50 characters):\n %.*s\n", 50, y);
    int result = strcmp(x, y);
    printf("Result of string comparison: %c\n", result == 0 ? 'T' : 'F');

    show_stats();

    int f9 = create_dir("/home/sayan/AOSD/Assignment3");
    printf("Writing to file /home/sayan/AOSD/Assignment3/hello.c\n");
    sprintf(x, "%s",
            "#define <stdio.h>\nint main() {\n printf(\"Hello World\");\n}");
    int f10 = write_file("/home/sayan/AOSD/Assignment3/hello.c", x,
                         strlen(x) + 1, 0);
    printf("Wrote %d bytes\n", f10);
    show_stats();

    printf("Reading from file /home/sayan/AOSD/Assignment3/hello.c\n");
    int f11 = read_file("/home/sayan/AOSD/Assignment3/hello.c", y,
                        strlen(x) + 1, 0);
    printf("Read %d bytes\n", f11);
    printf("Read :\n%s\n", y);
    result = strcmp(x, y);
    printf("Result of string comparison: %c\n", result == 0 ? 'T' : 'F');

    printf("Removing /home/sayan/AOSD \n");
    int f6 = remove_dir("/home/sayan/AOSD");
    printf("Status of remove operation: %d\n", f6);
    show_stats();

    printf("Removing everything \n");
    int f7 = remove_dir("/");
    printf("Status of remove operation: %d\n", f7);
    show_stats();
}

int main() {
    test_0();
    return 0;
}
