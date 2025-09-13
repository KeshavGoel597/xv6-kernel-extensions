#include "kernel/types.h"
#include "user.h"
#include "kernel/fcntl.h"

int main(int argc, char *argv[]) {
    int fd, n;
    char buf[100];
    int before, after;

    before = getreadcount();
    printf("Initial readcount: %d\n", before);

    fd = open("README", O_RDONLY);
    if(fd < 0) {
        printf("Failed to open file\n");
        exit(1);
    }
    n = read(fd, buf, 100);
    if(n < 0) {
        printf("Read failed\n");
        close(fd);
        exit(1);
    }
    close(fd);

    after = getreadcount();
    printf("Readcount after reading 100 bytes: %d\n", after);
    printf("Bytes read (should be 100): %d\n", after - before);
    exit(0);
}
