#include "shm.h"
#include "types.h"
#include "stat.h"
#include "user.h"
#include "shmuser.h"

int
main(void)
{
    int shmid;
    int shmid2;
    int shmid3;
    int shmid4;
    char *shared;
    char *shared2;
    char *shared3;
    char *shared4;
    int pid;
    int status;

    printf(1, "\n===== SHMAT TEST =====\n");

    // Create a shared-memory segment.
    shmid = shmget(1234, 4096, IPC_CREAT | 0666);
    printf(1, "shmget returned: %d\n", shmid);

    if (shmid < 0) {
        printf(1, "FAIL: Could not create shared-memory segment\n");
        exit();
    }

    // Attach the segment.
    shared = (char *)shmat(shmid, 0 , 0);

    if ((uint)shared == (uint)-1) {
        printf(1, "FAIL: shmat could not attach the segment\n");
        exit();
    }

    printf(1, "Attached at address: %p\n", shared);

    // Write and read data.
    shared[0] = 'H';
    shared[1] = 'i';
    shared[2] = '\0';

    if (shared[0] == 'H' &&
        shared[1] == 'i' &&
        shared[2] == '\0') {
        printf(1, "PASS: Read/write shared memory\n");
    } else {
        printf(1, "FAIL: Read/write shared memory\n");
    }

    // Attaching the same segment twice should fail.
    shared2 = (char *)shmat(shmid, 0, 0);

    if ((uint)shared2 == (uint)-1) {
        printf(1, "PASS: Duplicate attachment rejected\n");
    } else {
        printf(1, "FAIL: Duplicate attachment was allowed\n");
    }

    // Create a second, independent segment.
    shmid2 = shmget(5678, 4096, IPC_CREAT | 0666);

    if (shmid2 < 0) {
        printf(1, "FAIL: Could not create second segment\n");
        exit();
    }

    shared2 = (char *)shmat(shmid2, 0, 0);

    if ((uint)shared2 == (uint)-1) {
        printf(1, "FAIL: Could not attach second segment\n");
        exit();
    }

    shared2[0] = 'X';

    if (shared[0] == 'H' && shared2[0] == 'X') {
        printf(1, "PASS: Independent segments hold separate data\n");
    } else {
        printf(1, "FAIL: Independent segment test\n");
    }

    //Test RND Flag
    printf(1, "===== SHM_RND TEST =====\n");
    shmid3 = shmget(100, 4096, IPC_CREAT | 0666);
    printf(1, "shmget returned: %d\n", shmid3);

    if (shmid3 < 0) {
        printf(1, "FAIL: Could not create shared-memory segment\n");
        exit();
    }

    // Attach the segment.
    shared3 = (char *)shmat(shmid3, 0x71000123, SHM_RND);

    if ((uint)shared3 != 0x71000000) {
        printf(1, "FAIL: SHM_RND Failed\n");
        exit();
    }

    printf(1, "Rounding Successful: %p\n", shared3);
    shared3[0] = 'X';
    if (shared3[0] != 'X') {
        printf(1, "FAIL: Read/write shared memory\n");
        exit();
    }
    printf(1, "PASS: Read/write shared memory\n");

    //Test RD_ONLY memory
    /*printf(1, "===== SHM_RD_ONLY TEST =====\n");
    shmid = shmget(200, 4096, IPC_CREAT | 0666);
    shared4 = (char*)shmat(shmid, 0, SHM_RDONLY);
    if ((uint)shared4 == (uint)-1) {
        printf(1, "FAIL: SHM_RD_ONLY Failed\n");
        exit();
    }
    shared4[0] = 'X';
    if (shared4[0] == 'X') {
        printf(1, "FAIL: Read only shared memory\n");
        exit();
    }
    printf(1, "PASS: Read/write shared memory\n");*/

    // Test shared-memory inheritance across fork.
    pid = fork();

    if (pid < 0) {
        printf(1, "FAIL: fork failed\n");
        exit();
    }

    if (pid == 0) {
        // The child should inherit the attachment at the same address.
        if (shared[0] != 'H') {
            printf(1, "FAIL: Child cannot read parent's shared data\n");
            exit();
        }

        shared[0] = 'C';

        printf(1, "Child wrote: %c\n", shared[0]);
        exit();
    }

    wait();

    // The child's write should be visible in the parent.
    if (shared[0] == 'C') {
        printf(1, "PASS: Shared memory works across fork\n");
    } else {
        printf(1, "FAIL: Child's write not visible to parent\n");
    }

    printf(1, "===== SHMAT TEST COMPLETE =====\n");

    exit();
}
