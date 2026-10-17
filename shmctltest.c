#include "types.h"
#include "stat.h"
#include "user.h"
#include "shm.h"

#define TEST_KEY 5001
#define TEST_SIZE 4096

static int passed = 0;
static int failed = 0;

static void
check(char *name, int condition)
{
    if (condition) {
        printf(1, "PASS: %s\n", name);
        passed++;
    } else {
        printf(1, "FAIL: %s\n", name);
        failed++;
    }
}

int
main(void)
{
    int shmid;
    int shmid2;
    int ret;
    int i;
    int found;
    int stat_shmid;
    char *addr;
    char *addr2;
    struct shmid_ds ds;
    struct shmid_ds stat_ds;
    struct shm_info info_before;
    struct shm_info info_after;
    struct shm_limits limits;

    printf(1, "\n====================================\n");
    printf(1, "       SHMCTL COMPREHENSIVE TEST\n");
    printf(1, "====================================\n");

    //TEST 1: Create a shared-memory segment.
    printf(1, "\n===== TEST 1: Segment Creation =====\n");

    shmid = shmget(TEST_KEY, TEST_SIZE, IPC_CREAT | 0666);

    check("Create shared memory segment", shmid >= 0);

    if (shmid < 0) {
        printf(1, "Cannot continue without a shared-memory segment.\n");
        exit();
    }

    printf(1, "Created segment ID: %d\n", shmid);

    //TEST 2: IPC_STAT
    printf(1, "\n===== TEST 2: IPC_STAT =====\n");

    ret = shmctl(shmid, IPC_STAT, &ds);

    check("IPC_STAT succeeds", ret == 0);

    if (ret == 0) {
        printf(1, "Segment ID: %d\n", ds.shmid);
        printf(1, "Key: %d\n", ds.key);
        printf(1, "Size: %d\n", ds.shm_size);
        printf(1, "Permissions: %d\n", ds.permissions);
        printf(1, "Attachments: %d\n", ds.nattch);
        printf(1, "Marked for deletion: %d\n", ds.marked_delete);

        check("IPC_STAT returns correct shmid",
              ds.shmid == shmid);

        check("IPC_STAT returns correct key",
              ds.key == TEST_KEY);

        check("IPC_STAT returns correct size",
              ds.shm_size == TEST_SIZE);

        check("IPC_STAT returns correct permissions",
              ds.permissions == 0666);

        check("New segment has zero attachments",
              ds.nattch == 0);

        check("New segment is not marked for deletion",
              ds.marked_delete == 0);
    }

    // TEST 3: IPC_SET
    printf(1, "\n===== TEST 3: IPC_SET =====\n");

    ds.permissions = 0600;

    ret = shmctl(shmid, IPC_SET, &ds);

    check("IPC_SET updates permissions", ret == 0);

    ret = shmctl(shmid, IPC_STAT, &ds);

    check("IPC_STAT succeeds after IPC_SET", ret == 0);

    if (ret == 0) {
        printf(1, "Updated permissions: %d\n", ds.permissions);

        check("Permissions changed to 0600",
              ds.permissions == 0600);
    }

    //TEST 4: Reject invalid permission bits.
    //Our IPC_SET implementation rejects bits outside 0777.
    printf(1, "\n===== TEST 4: Invalid IPC_SET =====\n");

    ds.permissions = 01000;

    ret = shmctl(shmid, IPC_SET, &ds);

    check("IPC_SET rejects invalid permission bits",
          ret < 0);

    ret = shmctl(shmid, IPC_STAT, &ds);

    check("IPC_STAT succeeds after rejected IPC_SET",
          ret == 0);

    if (ret == 0) {
        check("Rejected IPC_SET preserves permissions",
              ds.permissions == 0600);
    }

    //TEST 5: IPC_INFO
    printf(1, "\n===== TEST 5: IPC_INFO =====\n");

    ret = shmctl(0, IPC_INFO, &limits);

    check("IPC_INFO succeeds", ret == 0);

    if (ret == 0) {
        printf(1, "SHMMAX: %d\n", limits.shmmax);
        printf(1, "SHMMIN: %d\n", limits.shmmin);
        printf(1, "SHMMNI: %d\n", limits.shmmni);
        printf(1, "Max pages per segment: %d\n",
               limits.max_pages_per_segment);

        check("SHMMAX is nonzero", limits.shmmax > 0);
        check("SHMMIN is nonzero", limits.shmmin > 0);
        check("SHMMNI is nonzero", limits.shmmni > 0);
        check("Maximum pages per segment is nonzero",
              limits.max_pages_per_segment > 0);

        check("SHMMAX is at least SHMMIN",
              limits.shmmax >= limits.shmmin);
    }

    //TEST 6: SHM_INFO
    printf(1, "\n===== TEST 6: SHM_INFO =====\n");

    ret = shmctl(0, SHM_INFO, &info_before);

    check("SHM_INFO succeeds", ret == 0);

    if (ret == 0) {
        printf(1, "Used IDs: %d\n", info_before.used_ids);
        printf(1, "Total pages: %d\n", info_before.total_pages);
        printf(1, "Total attachments: %d\n",
               info_before.total_attached);

        check("SHM_INFO reports at least one segment",
              info_before.used_ids >= 1);

        check("SHM_INFO reports nonzero allocated pages",
              info_before.total_pages >= 1);

        check("SHM_INFO reports nonnegative attachments",
              info_before.total_attached >= 0);
    }

    //TEST 7: SHM_STAT
    //In this implementation, SHM_STAT takes a table index,
    //not a shmid. Search the table for our segment.
    printf(1, "\n===== TEST 7: SHM_STAT =====\n");

    found = 0;

    for (i = 0; i < SHM_TABLE_MAX; i++) {
        ret = shmctl(i, SHM_STAT, &stat_ds);

        if (ret >= 0 && stat_ds.shmid == shmid) {
            found = 1;
            stat_shmid = ret;
            break;
        }
    }

    check("SHM_STAT finds the created segment", found);

    if (found) {
        printf(1, "SHM_STAT returned ID: %d\n", stat_shmid);

        check("SHM_STAT returns the correct segment ID",
              stat_shmid == shmid);

        check("SHM_STAT returns the correct size",
              stat_ds.shm_size == TEST_SIZE);

        check("SHM_STAT returns the correct key",
              stat_ds.key == TEST_KEY);
    }

    //TEST 8: SHM_STAT_ANY
    printf(1, "\n===== TEST 8: SHM_STAT_ANY =====\n");

    found = 0;

    for (i = 0; i < SHM_TABLE_MAX; i++) {
        ret = shmctl(i, SHM_STAT_ANY, &stat_ds);

        if (ret >= 0 && stat_ds.shmid == shmid) {
            found = 1;
            break;
        }
    }

    check("SHM_STAT_ANY finds the created segment", found);

    if (found) {
        check("SHM_STAT_ANY returns the correct size",
              stat_ds.shm_size == TEST_SIZE);
    }

    //TEST 9: Verify attachment count changes.
    printf(1, "\n===== TEST 9: Attachment Accounting =====\n");

    addr = (char *)shmat(shmid, 0, 0);

    check("shmat attaches successfully",
          (uint)addr != (uint)-1);

    if ((uint)addr != (uint)-1) {
        ret = shmctl(shmid, IPC_STAT, &ds);

        check("IPC_STAT succeeds while attached", ret == 0);

        if (ret == 0) {
            printf(1, "Attachments after shmat: %d\n", ds.nattch);

            check("Attachment count increases to one",
                  ds.nattch == 1);
        }

        addr[0] = 'A';

        check("Attached memory is writable", addr[0] == 'A');

        ret = shmdt((uint) addr);

        check("shmdt detaches successfully", ret == 0);

        ret = shmctl(shmid, IPC_STAT, &ds);

        check("IPC_STAT succeeds after detach", ret == 0);

        if (ret == 0) {
            printf(1, "Attachments after shmdt: %d\n", ds.nattch);

            check("Attachment count returns to zero",
                  ds.nattch == 0);
        }
    }

    //TEST 10: IPC_RMID while attached.
    //Existing attachments should remain usable after IPC_RMID.
    //New attachments should be rejected.
    //The segment should be freed after the last detach.
    printf(1, "\n===== TEST 10: IPC_RMID While Attached =====\n");

    addr = (char *)shmat(shmid, 0, 0);

    check("Attach before IPC_RMID",
          (uint)addr != (uint)-1);

    if ((uint)addr != (uint)-1) {
        addr[0] = 'X';

        ret = shmctl(shmid, IPC_RMID, 0);

        check("IPC_RMID succeeds while attached", ret == 0);

        ret = shmctl(shmid, IPC_STAT, &ds);

        check("IPC_STAT finds a marked segment", ret == 0);

        if (ret == 0) {
            check("Segment is marked for deletion",
                  ds.marked_delete == 1);

            check("Attachment remains counted after IPC_RMID",
                  ds.nattch == 1);
        }

        check("Existing attachment remains readable",
              addr[0] == 'X');

        addr2 = (char *)shmat(shmid, 0, 0);

        check("New attachment is rejected after IPC_RMID",
              (uint)addr2 == (uint)-1);

        ret = shmdt((uint) addr);

        check("Final shmdt succeeds after IPC_RMID",
              ret == 0);

        //After the last detach, the segment should be freed.
        ret = shmctl(shmid, IPC_STAT, &ds);

        check("Segment is freed after final detach",
              ret < 0);
    } else {
        //Cleanup if the attachment failed unexpectedly.
        ret = shmctl(shmid, IPC_RMID, 0);
        check("IPC_RMID cleanup", ret == 0);
    }

    //TEST 11: IPC_RMID without attachments.
    printf(1, "\n===== TEST 11: IPC_RMID Without Attachments =====\n");

    shmid2 = shmget(TEST_KEY + 1, TEST_SIZE, IPC_CREAT | 0666);

    check("Create second segment", shmid2 >= 0);

    if (shmid2 >= 0) {
        ret = shmctl(shmid2, IPC_STAT, &ds);

        check("Second segment initially exists", ret == 0);

        ret = shmctl(shmid2, IPC_RMID, 0);

        check("IPC_RMID succeeds without attachments", ret == 0);

        ret = shmctl(shmid2, IPC_STAT, &ds);

        check("Segment is immediately freed",
              ret < 0);
    }

    //TEST 12: Invalid shmid and command.
    printf(1, "\n===== TEST 12: Invalid Inputs =====\n");

    ret = shmctl(-1, IPC_STAT, &ds);

    check("IPC_STAT rejects invalid shmid", ret < 0);

    ret = shmctl(99999, IPC_STAT, &ds);

    check("IPC_STAT rejects nonexistent shmid", ret < 0);

    ret = shmctl(shmid, 999, &ds);

    check("shmctl rejects unsupported command", ret < 0);

    //TEST 13: Invalid table indices for SHM_STAT.
    printf(1, "\n===== TEST 13: Invalid Table Indices =====\n");

    ret = shmctl(-1, SHM_STAT, &stat_ds);

    check("SHM_STAT rejects negative table index",
          ret < 0);

    ret = shmctl(SHM_TABLE_MAX, SHM_STAT, &stat_ds);

    check("SHM_STAT rejects out-of-range table index",
          ret < 0);

    //Final results.
    printf(1, "\n====================================\n");
    printf(1, "          SHMCTL TEST RESULTS\n");
    printf(1, "====================================\n");
    printf(1, "Passed: %d\n", passed);
    printf(1, "Failed: %d\n", failed);

    if (failed == 0) {
        printf(1, "ALL SHMCTL TESTS PASSED!\n");
    } else {
        printf(1, "SOME SHMCTL TESTS FAILED!\n");
    }

    printf(1, "====================================\n");

    exit();
}