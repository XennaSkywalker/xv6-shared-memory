#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "fcntl.h"
#include "defs.h"
#include "shm.h"
#include "shmuser.h"

extern struct shm_table shm_table;

// Helper function validates the name used via fd's to access the shared mem segment
static int
shm_posix_valid_name(const char *name)
{
    if (name == 0 || name[0] != '/') return 0;
    if (name[1] == '\0') return 0;

    for (int i = 1; i < SHM_NAME_MAX; i++) {
        if (name[i] == '\0') return 1;
        if (name[i] == '/') return 0;
    }

    return 0;
}

//Locks the global shm segment table to access the shm segment via its POSIX name
static struct shmseg*
shm_posix_find_locked(const char *name)
{
    struct shmseg *seg;
    for (int i = 0; i < SHM_TABLE_MAX; i++) {
        seg = &shm_table.seg[i];
        if (!seg->used) continue;
        if (!seg->is_posix) continue;
        if (seg->marked_delete) continue;

        if (strncmp(seg->name, name, SHM_NAME_MAX) == 0) return seg;
    }

    return 0;
}

//Creates a POSIX shared-memory object.
//Caller must hold shm_table.lock
static struct shmseg *
shm_posix_create_locked(const char *name, int mode)
{
    struct shmseg *seg;
    int i;

    for (i = 0; i < SHM_TABLE_MAX; i++) {
        if (!shm_table.seg[i].used)
            break;
    }

    if (i == SHM_TABLE_MAX)
        return 0;

    seg = &shm_table.seg[i];

    memset(seg, 0, sizeof(*seg));

    seg->used = 1;
    seg->shmid = shm_alloc_id_locked();
    seg->key = 0;
    seg->shm_size = 0;
    seg->permissions = mode & 0777;
    seg->tot_attached = 0;
    seg->marked_delete = 0;
    seg->is_posix = 1;
    seg->open_refs = 0;

    safestrcpy(seg->name, name, SHM_NAME_MAX);

    return seg;
}

//Dont know for now
static struct file *
shm_file_alloc(void)
{
    struct file *f;
    f = filealloc();
    if (f == 0)
        return 0;
    return f;
}

//Allocates first free fd from the proc's ofile struct.
static int
shm_fd_alloc(struct proc *p, struct file *f)
{
    int fd;
    if (p == 0 || f == 0)
        return -1;

    for (fd = 0; fd < NOFILE; fd++) {
        if (p->ofile[fd] == 0) {
            p->ofile[fd] = f;
            return fd;
        }
    }
    return -1;
}

//Wrapper syscall that calls fileclose() which has been modified to call shm_free() when fd is of enum type FD_SHM
static void
shm_file_free(struct file *f)
{
    if (f == 0) return;
    fileclose(f);
}

//Wrapper to call shm_free() from shm_unlink()
static void
shm_destroy_locked(struct shmseg* seg)
{
    if (seg == 0) return;
    if (!seg->marked_delete) return;
    shm_free(seg);
}

//POSIX shm_open().
//Returns a file descriptor on success, or -1 on failure.
int
shm_open(const char *name, int oflag, int mode)
{
    struct proc *p;
    struct file *f;
    struct shmseg *seg;
    int fd;
    int access_mode;
    int i;

    p = myproc();

    if (p == 0 || !shm_posix_valid_name(name))
        return -1;

    //Flag checks to check whether correct flags and correct order of flags has been used
    access_mode = oflag & O_ACCMODE;

    if (access_mode != O_RDONLY && access_mode != O_RDWR)
        return -1;

    if (oflag & ~(O_ACCMODE | O_CREAT | O_EXCL))
        return -1;

    if ((oflag & O_EXCL) && !(oflag & O_CREAT))
        return -1;

    //Check for an available file descriptor first.
    for (i = 0; i < NOFILE; i++) {
        if (p->ofile[i] == 0)
            break;
    }

    if (i == NOFILE)
        return -1;

    f = shm_file_alloc();

    if (f == 0)
        return -1;

    acquire(&shm_table.lock);

    //Finds the shared segment if it exists.
    seg = shm_posix_find_locked(name);

    if (seg != 0) {
        //If shared segment is already present.
        //O_CREAT and O_EXCL in combination explicitly state to create a new shared object if it doesn't exist which is not necessary in this case.
        if ((oflag & O_CREAT) && (oflag & O_EXCL)) {
            release(&shm_table.lock);
            shm_file_free(f);
            return -1;
        }
    }
    //If shared segment doesn't exsist...then O_CREAT flag must be present.
    else {
        if (!(oflag & O_CREAT)) {
            release(&shm_table.lock);
            shm_file_free(f);
            return -1;
        }

        seg = shm_posix_create_locked(name, mode);

        if (seg == 0) {
            release(&shm_table.lock);
            shm_file_free(f);
            return -1;
        }
    }

    seg->open_refs++;

    f->type = FD_SHM;
    f->readable = 1;
    f->writable = (access_mode == O_RDWR);
    f->pipe = 0;
    f->ip = 0;
    f->shm = seg;
    f->off = 0;

    release(&shm_table.lock);

    //Install the file in the available descriptor slot.
    fd = shm_fd_alloc(p, f);

    if (fd < 0) {
        shm_file_free(f);
        return -1;
    }

    return fd;
}

//POSIX shm_unlink().
//Removes the name of a POSIX shared-memory object.
//Existing file descriptors and attachments remain valid.
//Returns 0 on success, or -1 on failure.
int
shm_unlink(const char *name)
{
    struct proc *p;
    struct shmseg *seg;

    p = myproc();

    if (p == 0 || !shm_posix_valid_name(name)) return -1;

    acquire(&shm_table.lock);
    //Search for the shared segment via its name.
    seg = shm_posix_find_locked(name);
    if (seg == 0) {
        release(&shm_table.lock);
        return -1;
    }

    //Remove the object's name from the namespace.
    seg->marked_delete = 1;

    //Destroy immediately only if no file references
    //or shared-memory attachments remain.
    if (seg->open_refs == 0 && seg->tot_attached == 0)
        shm_destroy_locked(seg);

    release(&shm_table.lock);
    return 0;
}


