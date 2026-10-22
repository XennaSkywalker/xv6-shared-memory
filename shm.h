//this header file contains kernel side info

#ifndef SHM_H
#define SHM_H

#include "types.h"
#include "spinlock.h"
#include "shmuser.h"

#define SHMMNI 64
#define SHM_TABLE_MAX SHMMNI
#define SHM_MAX_PAGES 256
#define SHM_MAX_ATTACHED 16
#define SHM_BASE 0x70000000
#define SHMLBA 4096
#define SHMMIN 1
#define SHMMAX (SHM_MAX_PAGES * 4096)
#define SHM_NAME_MAX 32


struct proc;
extern struct shm_table shm_table;

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
    int is_posix;
    char name[SHM_NAME_MAX];
    int open_refs;
};

struct shm_table {
    struct spinlock lock;
    struct shmseg seg[SHM_TABLE_MAX];
};

struct shm_attachment {
    int used;
    int shmid;
    uint addr;
    uint length;
    uint offset;
    int prot;
    int flags;
};

void shm_init(void);

struct shmseg* shm_find_by_key(int key);
struct shmseg* shm_find_by_shmid(int shmid);

int shm_create(int key, uint size, int permissions);
void shm_free(struct shmseg* seg);
int shm_free_pages(struct shmseg *seg);
int shm_alloc_pages(struct shmseg *seg);
int shm_fork(struct proc *parent, struct proc *child);
void shm_detach_all(struct proc *p);
void shm_exec_detach(struct proc *p, pde_t *oldpgdir);
int shm_addr_free(struct proc *p, uint addr, uint npages);
int shm_unmap_pages(pde_t *pgdir, uint addr, uint npages);

int shm_get(int key, uint size, int shmflg);
int shm_at(int shmid, uint addr, int shmflags);
int shm_dt(uint shmaddr);
int shm_ctl(int shmid, int cmd, void* buf);

//POSIX syscalls
int shm_open(const char* name, int oflag, int mode);
int shm_alloc_id_locked(void);
int shm_alloc_id(void);
int shm_unlink(const char* name);
int shm_mmap(int fd, uint addr, int length, int prot, int flags, uint offset);

#endif
