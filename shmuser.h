//this is the header file to be included in user programs

#ifndef SHMUSER_H
#define SHMUSER_H

#define IPC_PRIVATE 0
#define IPC_CREAT   01000
#define IPC_EXCL    02000
#define SHM_RDONLY  0x01
#define SHM_RND     0x02
#define SHM_REMAP   040000
#define SHM_EXEC    0100000

#define IPC_STAT    01000
#define IPC_SET     02000
#define IPC_RMID    03000
#define IPC_INFO    04000
#define SHM_INFO    05000
#define SHM_STAT    06000
#define SHM_STAT_ANY  07000

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
