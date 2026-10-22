//this is the header file to be included in user programs

#ifndef SHMUSER_H
#define SHMUSER_H

/* System V IPC flags */
#define IPC_PRIVATE     0x0000
#define IPC_CREAT       0x0001
#define IPC_EXCL        0x0002

/* System V shared-memory attachment flags */
#define SHM_RDONLY      0x0004
#define SHM_RND         0x0008
#define SHM_REMAP       0x0010
#define SHM_EXEC        0x0020

/* POSIX shm_open flags */
#define O_ACCMODE       0x0003
#define O_CREAT         0x0004
#define O_EXCL          0x0008

/* Shared-memory control commands */
#define IPC_STAT        0x0001
#define IPC_SET         0x0002
#define IPC_RMID        0x0003
#define IPC_INFO        0x0004
#define SHM_INFO        0x0005
#define SHM_STAT        0x0006
#define SHM_STAT_ANY    0x0007

/* POSIX mmap flags */
#define PROT_NONE   0x0
#define PROT_READ   0x1
#define PROT_WRITE  0x2
#define PROT_EXEC   0x4

#define MAP_SHARED  0x01
#define MAP_PRIVATE 0x02
#define MAP_FIXED   0x10

struct shmid_ds {
    int shmid;
    int key;
    uint shm_size;
    int permissions;
    int nattch;
    int marked_delete;
};

struct shm_info {
    int used_ids;
    int total_pages;
    int total_attached;
};

struct shm_limits {
    uint shmmax;
    uint shmmin;
    int shmmni;
    int max_pages_per_segment;
};

#endif
