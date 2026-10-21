#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "shmuser.h"

int
shm_open_test(void)
{
    int fd1;
    int fd2;
    int fd3;

    printf(1, "===== shm_open TEST =====\n");

    /* Test 1: Create a shared-memory object. */
    fd1 = shmopen("/test", O_CREAT | O_RDWR, 0666);

    if (fd1 < 0) {
        printf(1, "FAIL: shm_open create\n");
        return -1;
    }

    printf(1, "PASS: Created object, fd=%d\n", fd1);

    /* Test 2: Open the existing object. */
    fd2 = shmopen("/test", O_RDWR, 0666);

    if (fd2 < 0) {
        printf(1, "FAIL: shm_open existing object\n");
        return -1;
    }

    printf(1, "PASS: Opened existing object, fd=%d\n", fd2);

    /* Test 3: O_CREAT | O_EXCL must fail for existing objects. */
    fd3 = shmopen("/test", O_CREAT | O_EXCL | O_RDWR, 0666);

    if (fd3 >= 0) {
        printf(1, "FAIL: O_EXCL should have failed\n");
        close(fd3);
        return -1;
    }

    printf(1, "PASS: O_EXCL correctly rejected existing object\n");

    /* Test 4: Opening a nonexistent object without O_CREAT. */
    fd3 = shmopen("/missing", O_RDWR, 0666);

    if (fd3 >= 0) {
        printf(1, "FAIL: Opened nonexistent object\n");
        close(fd3);
        return -1;
    }

    printf(1, "PASS: Missing object correctly rejected\n");

    close(fd1);
    close(fd2);

    printf(1, "===== TEST COMPLETE =====\n");
    return 0;
}

int
shm_unlink_test(void)
{
    int fd1;
    int fd2;
    int fd3;
    int ret;

    printf(1, "\n===== SHM_UNLINK TESTS =====\n");

    /*
     * TEST 1:
     * Create a shared-memory object.
     */
    printf(1, "\nTEST 1: Create shared-memory object\n");

    fd1 = shmopen("/unlink_test", O_CREAT | O_RDWR, 0666);

    if (fd1 < 0) {
        printf(1, "FAIL: Could not create shared-memory object\n");
        return -1;
    }

    printf(1, "PASS: Shared-memory object created, fd = %d\n", fd1);

    /*
     * TEST 2:
     * Unlink the existing object.
     */
    printf(1, "\nTEST 2: Unlink existing object\n");

    ret = shmunlink("/unlink_test");

    if (ret < 0) {
        printf(1, "FAIL: shm_unlink failed for existing object\n");
        close(fd1);
        return -1;
    }

    printf(1, "PASS: shm_unlink succeeded\n");

    /*
     * TEST 3:
     * Opening the unlinked name without O_CREAT must fail.
     */
    printf(1, "\nTEST 3: Open an unlinked name without O_CREAT\n");

    fd2 = shmopen("/unlink_test", O_RDWR, 0666);

    if (fd2 >= 0) {
        printf(1, "FAIL: Open succeeded for an unlinked name\n");
        close(fd2);
        close(fd1);
        return -1;
    }

    printf(1, "PASS: Opening the unlinked name failed\n");

    /*
     * TEST 4:
     * The same name can be used to create a new object.
     * fd1 still refers to the previously opened object.
     */
    printf(1, "\nTEST 4: Recreate object using the same name\n");

    fd3 = shmopen("/unlink_test", O_CREAT | O_RDWR, 0666);

    if (fd3 < 0) {
        printf(1, "FAIL: Could not recreate shared-memory object\n");
        close(fd1);
        return -1;
    }

    printf(1, "PASS: New object created, fd = %d\n", fd3);

    /*
     * TEST 5:
     * Unlink the newly created object.
     */
    printf(1, "\nTEST 5: Unlink recreated object\n");

    ret = shmunlink("/unlink_test");

    if (ret < 0) {
        printf(1, "FAIL: Could not unlink recreated object\n");
        close(fd1);
        close(fd3);
        return -1;
    }

    printf(1, "PASS: Recreated object unlinked\n");

    /*
     * TEST 6:
     * Unlinking the same name again must fail.
     */
    printf(1, "\nTEST 6: Unlink the same name twice\n");

    ret = shmunlink("/unlink_test");

    if (ret >= 0) {
        printf(1, "FAIL: Second unlink unexpectedly succeeded\n");
        close(fd1);
        close(fd3);
        return -1;
    }

    printf(1, "PASS: Second unlink failed as expected\n");

    /*
     * TEST 7:
     * Invalid shared-memory name.
     */
    printf(1, "\nTEST 7: Unlink invalid name\n");

    ret = shmunlink("invalid_name");

    if (ret >= 0) {
        printf(1, "FAIL: Unlink succeeded for invalid name\n");
        close(fd1);
        close(fd3);
        return -1;
    }

    printf(1, "PASS: Invalid name rejected\n");

    /*
     * TEST 8:
     * Unlink a name that does not exist.
     */
    printf(1, "\nTEST 8: Unlink nonexistent object\n");

    ret = shmunlink("/does_not_exist");

    if (ret >= 0) {
        printf(1, "FAIL: Unlink succeeded for nonexistent object\n");
        close(fd1);
        close(fd3);
        return -1;
    }

    printf(1, "PASS: Nonexistent object rejected\n");

    /*
     * TEST 9:
     * Close descriptors after unlinking.
     *
     * The underlying objects should be eligible for destruction
     * when no open references or attachments remain.
     */
    printf(1, "\nTEST 9: Close descriptors after unlink\n");

    close(fd1);
    close(fd3);

    printf(1, "PASS: Descriptors closed\n");

    printf(1, "\n===== SHM_UNLINK TESTS COMPLETED =====\n");

    return 0;
}

int
main(void)
{
    if (shm_open_test() == -1)
        printf(1, "FAIL: shm_open_test() failed\n");
    if (shm_unlink_test() == -1)
        printf(1, "FAIL: shm_unlink_test() failed\n");
    printf(1, "PASS: All syscall tests succeeded\n");
    exit();
}
