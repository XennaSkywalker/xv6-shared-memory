#include "types.h"
#include "stat.h"
#include "user.h"
#include "shmuser.h"

int
main(void)
{
    int id1, id2, id3, id4, id5;

    // Create a new segment.
    id1 = shmget(1234, 4096, IPC_CREAT | 0666);
    printf(1, "Create segment: %d\n", id1);

    // Find the same segment.
    id2 = shmget(1234, 4096, 0666);
    printf(1, "Find existing segment: %d\n", id2);

    // Exclusive creation should fail for an existing key.
    id3 = shmget(1234, 4096, IPC_CREAT | IPC_EXCL | 0666);
    printf(1, "Exclusive creation: %d (expected -1)\n", id3);

    // Looking up a nonexistent key without IPC_CREAT should fail.
    id4 = shmget(5678, 4096, 0666);
    printf(1, "Missing segment: %d (expected -1)\n", id4);

    // IPC_PRIVATE should create independent segments.
    id1 = shmget(0, 4096, 0666);
    id5 = shmget(0, 4096, 0666);

    printf(1, "Private segment 1: %d\n", id1);
    printf(1, "Private segment 2: %d\n", id5);

    if (id1 >= 0 && id5 >= 0 && id1 != id5)
        printf(1, "IPC_PRIVATE test passed\n");
    else
        printf(1, "IPC_PRIVATE test failed\n");

    exit();
}
