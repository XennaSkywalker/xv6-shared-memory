//this header file contains kernel side info

#ifndef SHM_H
#define SHM_H

#include "types.h"
#include "spinlock.h"
#include "shmuser.h"

#define SHM_TABLE_MAX 64
#define SHM_MAX_PAGES 256

//we dont enforce permissions rn, need to enforce them
struct shmseg {
    int used; 
    int shmid;
    int key;
    uint shm_size;
    int permissions;
    int tot_attached;
    int marked_delete;
    uint pages[SHM_MAX_PAGES];
};

struct shm_table {
    struct spinlock lock;
    struct shmseg seg[SHM_TABLE_MAX];
};

void shm_init(void);

struct shmseg* shm_find_by_key(int key);
struct shmseg* shm_find_by_shmid(int shmid);

int shm_create(int key, uint size, int permissions);
void shm_free(struct shmseg* seg);
int shm_free_pages(struct shmseg *seg);
int shm_alloc_pages(struct shmseg *seg);

int shm_get(int key, uint size, int shmflg);

#endif
