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

/*
 * Creates a POSIX shared-memory object.
 * Caller must hold shm_table.lock.
 */
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

static struct file *
shm_file_alloc(void)
{
    struct file *f;
    f = filealloc();
    if (f == 0)
        return 0;
    return f;
}

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

static void
shm_file_free(struct file *f)
{
    if (f == 0) return;
    fileclose(f);
}

/*
 * POSIX shm_open().
 *
 * Returns a file descriptor on success, or -1 on failure.
 */
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

    access_mode = oflag & O_ACCMODE;

    if (access_mode != O_RDONLY && access_mode != O_RDWR)
        return -1;

    if (oflag & ~(O_ACCMODE | O_CREAT | O_EXCL))
        return -1;

    if ((oflag & O_EXCL) && !(oflag & O_CREAT))
        return -1;

    /* Check for an available file descriptor first. */
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

    seg = shm_posix_find_locked(name);

    if (seg != 0) {
        if ((oflag & O_CREAT) && (oflag & O_EXCL)) {
            release(&shm_table.lock);
            shm_file_free(f);
            return -1;
        }
    } else {
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

    /* Install the file in the available descriptor slot. */
    fd = shm_fd_alloc(p, f);

    if (fd < 0) {
        shm_file_free(f);
        return -1;
    }

    return fd;
}
