//this is the header file to be included in user programs

#ifndef SHMUSER_H
#define SHMUSER_H

#define IPC_PRIVATE 0
#define IPC_CREAT   01000
#define IPC_EXCL    02000

#define IPC_STAT 1
#define IPC_RMID 2

struct shmid_ds {
    int shmid;
    int key;
    unsigned int shm_size;
    int tot_attached;
};

#endif
