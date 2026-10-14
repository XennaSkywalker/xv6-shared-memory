#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "shm.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

int
sys_shmget(void)
{
    int key;
    int size;
    int shmflg;

    if (argint(0, &key) < 0)
        return -1;

    if (argint(1, &size) < 0)
        return -1;

    if (argint(2, &shmflg) < 0)
        return -1;

    if (size <= 0)
        return -1;

    return shm_get(key, (uint)size, shmflg);
}

int
sys_shmat(void)
{
  int shmid;
  uint addr;
  int shmflg;

  if (argint(0, &shmid) < 0)
    return -1;
  if (argint(1, &addr) < 0)
    return -1;
  if (argint(2, &shmflg) < 0)
    return -1;

  return shm_at(shmid, addr, shmflg);
}

int
sys_shmdt(void)
{
  uint shmaddr;

  if (argint(0, (int*)&shmaddr) < 0)
    return -1;

  return shm_dt(shmaddr);
}

int
sys_shmctl(void)
{
  cprintf("sys_shmctl called\n");
  return 0;
}
