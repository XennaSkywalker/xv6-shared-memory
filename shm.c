#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "shm.h"

struct shm_table shm_table;
static int next_shmid = 1;

void
shm_init(void)
{
    int i;
    initlock(&shm_table.lock, "shm");
    acquire(&shm_table.lock);

    for (i = 0; i < SHM_MAX_PAGES; i++) {
        shm_table.seg[i].used = 0;
        shm_table.seg[i].shmid = -1;
        shm_table.seg[i].key = 0;
        shm_table.seg[i].size = 0;
        shm_table.seg[i].perm = 0;
        shm_table.seg[i].nattch = 0;
        shm_table.seg[i].marked_for_delete = 0;
    }

    release(&shm_table.lock);
}

struct shmseg*
shm_find_by_key(int shmid)
{
    int i;
    for (i = 0; i < SHM_MAX_PAGES; i++) {
        if (shm_table.seg[i].used &&
            shm_table.seg[i].key == key &&
            !shm_table.seg[i].marked_delete) {
            return &shm_table.seg[i];
        }
    }
    return 0;
}

struct shmseg*
shm_find_by_shmid(int shmid)
{
    int i;
    for(i = 0; i < SHM_MAX_PAGES; i++){
        if(shm_table.seg[i].used &&
           shm_table.seg[i].shmid == shmid &&
           !shm_table.seg[i].marked_delete) {
            return &shm_table.seg[i];
        }
    }
    return 0;
}

int
shm_create(int key, uint size, int permissions)
{
    int i,j;
    int shmid;

    if (size == 0)
        return -1;

    if ((size + PGSIZE - 1)/PGSIZE > SHM_MAX_PAGES)
        return -1;

    acquire(&shm_table.lock);
    for (i = 0; i < SHM_MAX_PAGES; i++) {
        if (!shm_table.seg[i].used)
            break;
    }
    if (i == SHM_MAX_PAGES) {
        release(&shm_table.lock);
        return -1;
    }

    shmid = next_shmid;
    next_shmid++;

    shm_table.seg[i].used = 1;
    shm_table.seg[i].shmid = shmid;
    shm_table.seg[i].key = key;
    shm_table.seg[i].shm_size = size;
    shm_table.seg[i].permissions = permissions;
    shm_table.seg[i].tot_attached = 0;
    shm_table.seg[i].marked_delete = 0;

    for (j = 0; j < SHM_MAX_PAGES; j++)
        shm_table.seg[i].used = 0;
    release(&shm_table.lock);
    return shmid;
}

void
shm_free_segment(struct shmseg *seg)
{
    int i;
    if(seg == 0)
        return;

    for(i = 0; i < SHM_MAX_PAGES; i++)
        seg->pages[i] = 0;

    seg->used = 0;
    seg->shmid = -1;
    seg->key = 0;
    seg->shm_size = 0;
    seg->permissions = 0;
    seg->tot_attached = 0;
    seg->marked_delete = 0;
}