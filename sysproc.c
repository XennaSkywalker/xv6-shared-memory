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

  return shm_at(shmid, (uint) addr, shmflg);
}

int
sys_shmdt(void)
{
  uint shmaddr;

  if (argint(0, &shmaddr) < 0)
    return -1;

  return shm_dt((uint) shmaddr);

}

int
sys_shmctl(void)
{
  int shmid;
  int cmd;
  char *buf;
  int size;

  if (argint(0, &shmid) < 0)
    return -1;

  if (argint(1, &cmd) < 0)
    return -1;

  switch (cmd) {
    case IPC_STAT:
    case IPC_SET:
    case SHM_STAT:
    case SHM_STAT_ANY:
      size = sizeof(struct shmid_ds);
      break;

    case IPC_INFO:
      size = sizeof(struct shm_limits);
      break;

    case SHM_INFO:
      size = sizeof(struct shm_info);
      break;

    case IPC_RMID:
      return shm_ctl(shmid, cmd, 0);

    default:
      return -1;
  }

  if (argptr(2, &buf, size) < 0)
    return -1;

  return shm_ctl(shmid, cmd, buf);
}

int
sys_shmopen(void)
{
  char *name;
  int oflag;
  int mode;

  if (argstr(0, &name) < 0)
    return -1;

  if (argint(1, &oflag) < 0)
    return -1;

  if (argint(2, &mode) < 0)
    return -1;

  return shm_open(name, oflag, mode);
}

int
sys_shmunlink(void)
{
  char *name;

  if (argstr(0, &name) < 0)
    return -1;

  return shm_unlink(name);
}

int
sys_shmmmap(void)
{
  int fd;
  int length;
  int prot;
  int flags;
  int offset;
  int addr;

  if (argint(0, &addr) < 0 ||
      argint(1, &length) < 0 ||
      argint(2, &prot) < 0 ||
      argint(3, &flags) < 0 ||
      argint(4, &fd) < 0 ||
      argint(5, &offset) < 0)
    return -1;

  if (offset < 0) return -1;
  return shm_mmap(fd, (uint)addr, length, prot, flags, (uint)offset);
}

int sys_shmmunmap(void) {
    uint addr;
    int length;

    if (argint(0, &addr) < 0)
        return -1;

    if (argint(1, &length) < 0)
        return -1;

    return shm_munmap((uint) addr, length);
}

int
sys_ftruncate(void) {
  int fd;
  int length;

  if (argint(0, &fd) < 0)
    return -1;

  if (argint(1, &length) < 0)
    return -1;


  struct proc *p = myproc();
  if (p == 0 || fd < 0 || fd >= NOFILE || p->ofile[fd] == 0)
    return -1;
}
