#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "shm.h"

#include "../xv6-public/x86.h"

struct shm_table shm_table;
static int next_shmid = 1;

//shmget() acquires the lock.
//shm_find_by_key() and shm_find_by_shmid() assume the lock is already held.
//shm_create() assumes the lock is already held.
//shm_free() assumes the lock is held when modifying a segment in the global table.

//Searches for the shm segment for the global shared memory table using a given shmid
static struct shmseg *
shm_lookup(int shmid , int include_deleted) {
    for (int i = 0; i < SHM_TABLE_MAX; i++) {
        if (shm_table.seg[i].used &&
            shm_table.seg[i].shmid == shmid &&
            (include_deleted ||
             !shm_table.seg[i].marked_delete)) {
            return &shm_table.seg[i];
        }
    }
    return 0;
}

//Given the pgdir , addr and no of pages...unmap all the pages from the procs pgdir.
static void
shm_unmap_pages(pde_t *pgdir, uint addr, uint npages)
{
    uint i;

    for (i = 0; i < npages; i++) {
        pte_t *pte;

        pte = walkpgdir(
            pgdir,
            (void *)(addr + i * PGSIZE),
            0
        );

        if (pte && (*pte & PTE_P))
            *pte = 0;
    }
}

//Checks if a given range of VA is unallocated in the proc's pgdir.
static int
shm_range_free(pde_t *pgdir, uint addr, uint npages)
{
    uint i;
    pte_t *pte;

    for (i = 0; i < npages; i++) {
        pte = walkpgdir(pgdir,
                        (void *)(addr + i * PGSIZE), 0);

        if (pte != 0 && (*pte & PTE_P))
            return 0;
    }

    return 1;
}

//Detach or unmap all the shared memory segments from the proc's pgdir.
static void
shm_detach_from_pgdir(struct proc *p, pde_t *pgdir)
{
    struct shmseg *seg;
    struct shm_attachment *attachment;
    uint npages;
    uint i;

    if (p == 0 || pgdir == 0)
        return;

    acquire(&shm_table.lock);

    for (i = 0; i < SHM_MAX_ATTACHED; i++) {
        attachment = &p->shm_attachments[i];

        if (!attachment->used)
            continue;

        seg = shm_lookup(attachment->shmid, 1);

        if (seg != 0) {
            npages = (seg->shm_size + PGSIZE - 1) / PGSIZE;

            shm_unmap_pages(
                pgdir,
                attachment->addr,
                npages
            );

            if (seg->tot_attached > 0)
                seg->tot_attached--;

            if (seg->marked_delete &&
                seg->tot_attached == 0) {
                shm_free(seg);
                }
        }

        memset(attachment, 0, sizeof(*attachment));
    }

    release(&shm_table.lock);
}

//Checks if the shared memory segment occupied overlaps with any other existing shared memory segment
static int
shm_addr_free(struct proc *p, uint addr, uint npages)
{
    struct shmseg *other;
    struct shm_attachment *attachment;
    uint end;
    uint other_addr;
    uint other_npages;
    uint other_end;
    uint i;

    if (p == 0 || npages == 0)
        return 0;

    if (addr < SHM_BASE || addr >= KERNBASE)
        return 0;

    if (npages > (KERNBASE - addr) / PGSIZE)
        return 0;

    end = addr + npages * PGSIZE;

    //Addr must be greater than proc's normal allocation size.
    if (addr < p->sz)
        return 0;

    //Check existing shared memory attachments.
    for (i = 0; i < SHM_MAX_ATTACHED; i++) {
        attachment = &p->shm_attachments[i];

        if (!attachment->used)
            continue;

        other = shm_lookup(attachment->shmid, 1);

        if (other == 0)
            continue;

        other_addr = attachment->addr;

        other_npages =
            (other->shm_size + PGSIZE - 1) / PGSIZE;

        if (other_npages == 0)
            continue;

        if (other_addr >= KERNBASE ||
            other_npages > (KERNBASE - other_addr) / PGSIZE)
            return 0;

        other_end = other_addr + other_npages * PGSIZE;

        //Check if the other addr will overlap with the current addr we are trying to check.
        if (addr < other_end && other_addr < end)
            return 0;
    }

    //Check the actual page table.
    if (!shm_range_free(p->pgdir, addr, npages))
        return 0;

    return 1;
}

void
shm_init(void)
{
    int i,j;
    initlock(&shm_table.lock, "shm");
    acquire(&shm_table.lock);

    for (i = 0; i < SHM_TABLE_MAX; i++) {
        shm_table.seg[i].used = 0;
        shm_table.seg[i].shmid = -1;
        shm_table.seg[i].key = 0;
        shm_table.seg[i].shm_size = 0;
        shm_table.seg[i].permissions = 0;
        shm_table.seg[i].tot_attached = 0;
        shm_table.seg[i].marked_delete = 0;
        
        for (j = 0; j < SHM_MAX_PAGES; j++)
            shm_table.seg[i].pages[j] = 0;
    }

    release(&shm_table.lock);
}

//caller shouldve acquired lock on shm_table
struct shmseg*
shm_find_by_key(int key)
{
    int i;
    for (i = 0; i < SHM_TABLE_MAX; i++) {
        if (shm_table.seg[i].used &&
            shm_table.seg[i].key == key &&
            !shm_table.seg[i].marked_delete) {
            return &shm_table.seg[i];
        }
    }
    return 0;
}

//caller shouldve acquired lock on shm_table
//wont find a seg thats marked for deletion 
//will need a way to find seg marked for deletion while implementing shmctl(IPC_RMID) 
struct shmseg*
shm_find_by_shmid(int shmid)
{
    int i;
    for(i = 0; i < SHM_TABLE_MAX; i++){
        if(shm_table.seg[i].used &&
           shm_table.seg[i].shmid == shmid &&
           !shm_table.seg[i].marked_delete) {
            return &shm_table.seg[i];
        }
    }
    return 0;
}

int
shm_get(int key, uint size, int shmflg)
{
    struct shmseg *seg;
    int shmid;
    int permissions;

    if (size < SHMMIN || size > SHMMAX)
        return -1;

    permissions = shmflg & 0777;

    acquire(&shm_table.lock);

    //key == 0 signals IPC_PRIVATE which should always create a new segment
    if (key == 0) {
        shmid = shm_create(key, size, permissions);
        release(&shm_table.lock);
        return shmid;
    }
    
    //find segment
    seg = shm_find_by_key(key);

    //segment with given key found
    if (seg != 0) {
        //both IPC_CREAT and IPC_EXCL cannot be present when a segment is already present
        if ((shmflg & IPC_CREAT) && (shmflg & IPC_EXCL)) {
            release(&shm_table.lock);
            return -1;
        }

        //requested size > segment size, cannot happen
        if (size > seg->shm_size) {
            release(&shm_table.lock);
            return -1;
        }

        shmid = seg->shmid;
        release(&shm_table.lock);
        return shmid;
    }

    //no existing segment + no IPC_CREAT
    if (!(shmflg & IPC_CREAT)) {
        release(&shm_table.lock);
        return -1;
    }

    //IPC_CREAT is given
    //create the segment
    shmid = shm_create(key, size, permissions);

    release(&shm_table.lock);
    return shmid;
}


//assume that shmget already has a lock on shm_table
//no need to acquire that lock here

//this function only manages the metadate for a segment
//shm_alloc_pages is responsible for allocating pages
int
shm_create(int key, uint size, int permissions)
{
    int i,j;
    int shmid;

    if (size < SHMMIN || size > SHMMAX)
        return -1;

    for (i = 0; i < SHM_TABLE_MAX; i++) {
        if (!shm_table.seg[i].used)
            break;
    }
    if (i == SHM_TABLE_MAX) {
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
        shm_table.seg[i].pages[j] = 0;
    
    if (shm_alloc_pages(&shm_table.seg[i]) < 0) {
      shm_free(&shm_table.seg[i]);
      return -1;
    }
    
    return shmid;
}

//this will only clear the metadata for a seg
void
shm_free(struct shmseg *seg)
{
    if(seg == 0)
        return;
        
    if (seg->tot_attached > 0)
        return;
    
    shm_free_pages(seg); //this actually frees the pages
    
    seg->used = 0;
    seg->shmid = -1;
    seg->key = 0;
    seg->shm_size = 0;
    seg->permissions = 0;
    seg->tot_attached = 0;
    seg->marked_delete = 0;
}

//free the physical pages
int
shm_free_pages(struct shmseg *seg)
{
    int i;
    int npages;

    if (seg == 0)
        return -1;

    npages = (seg->shm_size + PGSIZE - 1) / PGSIZE;

    for (i = 0; i < npages; i++) {
        if (seg->pages[i] != 0) {
            kfree((char *)P2V(seg->pages[i]));
            seg->pages[i] = 0;
        }
    }

    return 0;
}

//allocate actual physical pages to the seg
int
shm_alloc_pages(struct shmseg *seg)
{
    int i;
    int npages;
    char *mem;
    
    if (seg == 0)
        return -1;

    npages = (seg->shm_size + PGSIZE - 1) / PGSIZE;

    for (i = 0; i < npages; i++) {
        mem = kalloc();

        if (mem == 0)
            goto bad;

        memset(mem, 0, PGSIZE);
        seg->pages[i] = V2P(mem);
    }

    return 0;

bad:
    while (i > 0) {
        i--;
        kfree(P2V(seg->pages[i]));
        seg->pages[i] = 0;
    }

    return -1;
}

int
shm_at(int shmid, uint shmaddr, int shmflg)
{
    struct proc *p;
    struct shmseg *seg;
    uint addr;
    uint npages;
    uint i;
    int slot;
    int perm;
    int found;
    int requested;
    int remap;

    p = myproc();

    if (p == 0)
        return -1;

    //Unsupported currently
    if (shmflg & SHM_EXEC | shmflg & SHM_REMAP)
        return -1;

    // Unsupported Flags cause exit.
    if (shmflg & ~(SHM_RDONLY | SHM_RND | SHM_REMAP | SHM_EXEC))
        return -1;

    //A zero address requests automatic placement.
    requested = (shmaddr != 0);
    remap = (shmflg & SHM_REMAP) != 0;

    if (remap && !requested)
        return -1;

    //If RND enabled round off to nearest lower page alignment.
    //If no RND and addr not page aligned exit.
    //Also checks if addr is in a valid virtual addr range
    if (requested) {
        if (shmflg & SHM_RND) {
            shmaddr &= ~(SHMLBA - 1);
        } else if (shmaddr % PGSIZE != 0) {
            return -1;
        }

        if (shmaddr < SHM_BASE ||
            shmaddr >= KERNBASE)
            return -1;
    }

    acquire(&shm_table.lock);

    //shm_lookup finds the requested shared memory segment with or without include_deleted flag.
    seg = shm_lookup(shmid, 0);

    if (seg == 0) {
        release(&shm_table.lock);
        return -1;
    }

    //Find the first available attachment slot.
    //Reject duplicate attachments.
    slot = -1;

    for (i = 0; i < SHM_MAX_ATTACHED; i++) {
        if (p->shm_attachments[i].used &&
            p->shm_attachments[i].shmid == shmid) {
            release(&shm_table.lock);
            return -1;
        }

        if (!p->shm_attachments[i].used && slot == -1)
            slot = i;
    }

    if (slot == -1) {
        release(&shm_table.lock);
        return -1;
    }

    //Calculate the number of pages required.
    npages = (seg->shm_size + PGSIZE - 1) / PGSIZE;

    if (npages == 0 || npages > SHM_MAX_PAGES) {
        release(&shm_table.lock);
        return -1;
    }

    //Start looking for a suitable virtual addr if not provided from SHM_BASE to KERNBASE by first found basis
    //If addr provided (after rounding if needed) use that addr instead and if allocation not possible exit.
    found = 0;

    if (requested) {
        addr = shmaddr;

        //shm_addr_free checks if addr + npages * PAGESIZE is free
        if (shm_addr_free(p, addr, npages))
            found = 1;
    } else {
        addr = SHM_BASE;

        //addr less than KERNBASE and limit addr + npages * PAGESIZE also below KERNBASE
        while (addr < KERNBASE &&
               npages <= (KERNBASE - addr) / PGSIZE) {

            //Again shm_addr_free used to check free allocation possible
            if (shm_addr_free(p, addr, npages)) {
                found = 1;
                break;
            }

            //if current addr not possible to allocate...increment addr by PAGESIZE for pg alignment
            if (addr > KERNBASE - PGSIZE)
                break;

            addr += PGSIZE;
        }
    }

    if (!found) {
        release(&shm_table.lock);
        return -1;
    }

    //SHM_REMAP replaces the existing mappings in the requested range.
    //It's only safe within the dedicated shared memory region.
    /*if (remap) {
        for (i = 0; i < npages; i++) {
            pte_t *pte;
            pte = walkpgdir(
                p->pgdir,
                (void *)(addr + i * PGSIZE),
                0
            );

            if (pte != 0 && (*pte & PTE_P))
                *pte = 0;
        }

        //Invalidate stale translations on CPU
        lcr3(V2P(p->pgdir));
    }*/

    //Permissions for shared memory are set.
    //Incase of SHM_RDONLY , read only flag is set.
    perm = PTE_U;

    if (!(shmflg & SHM_RDONLY))
        perm |= PTE_W;

    //Map the segment's existing physical pages to the available virtual address found.
    for (i = 0; i < npages; i++) {
        if (mappages(
                p->pgdir,
                (void *)(addr + i * PGSIZE),
                PGSIZE,
                seg->pages[i],
                perm) < 0) {

            //If all pages were unable to be mapped then rollback the exsisting mapped pages
            //Only remove the mappings from the procs pgdir . Do not deallocate the pages.
            shm_unmap_pages(p->pgdir, addr, i);

            release(&shm_table.lock);
            return -1;
        }
    }

    //Record the attachment only after all mappings have succeeded.
    p->shm_attachments[slot].used = 1;
    p->shm_attachments[slot].shmid = shmid;
    p->shm_attachments[slot].addr = addr;

    seg->tot_attached++;

    release(&shm_table.lock);

    //Return the starting virtual address.
    return (int)addr;
}

int shm_dt(uint shmaddr)
{
    struct proc *p;
    struct shmseg *seg;
    struct shm_attachment *attachment;
    uint npages;
    uint i;

    p = myproc();

    if (p == 0)
        return -1;

    acquire(&shm_table.lock);

    attachment = 0;

    for (i = 0; i < SHM_MAX_ATTACHED; i++) {
        if (p->shm_attachments[i].used &&
            p->shm_attachments[i].addr == shmaddr) {
            attachment = &p->shm_attachments[i];
            break;
        }
    }

    if (attachment == 0) {
        release(&shm_table.lock);
        return -1;
    }

    seg = shm_lookup(attachment->shmid, 1);

    if (seg == 0) {
        release(&shm_table.lock);
        return -1;
    }

    npages = (seg->shm_size + PGSIZE - 1) / PGSIZE;

    shm_unmap_pages(p->pgdir, attachment->addr, npages);

    memset(attachment, 0, sizeof(*attachment));

    if (seg->tot_attached > 0)
        seg->tot_attached--;

    if (seg->marked_delete && seg->tot_attached == 0)
        shm_free(seg);

    release(&shm_table.lock);
    return 0;
}

int
shm_ctl(int shmid, int cmd, void *buf)
{
    struct shmseg *seg;
    struct shmid_ds *ds;
    struct shm_info *info;
    struct shm_limits *limits;
    int i;
    int index;
    int used_ids;
    int total_pages;
    int total_attached;

    switch (cmd) {

    //IPC_STAT returns the metadata of the given shmid to the user by copying it into shmid_ds data structure.
    case IPC_STAT:
        if (buf == 0)
            return -1;

        acquire(&shm_table.lock);

        seg = shm_lookup(shmid, 1);

        if (seg == 0) {
            release(&shm_table.lock);
            return -1;
        }

        ds = (struct shmid_ds *)buf;

        ds->shmid = seg->shmid;
        ds->key = seg->key;
        ds->shm_size = seg->shm_size;
        ds->permissions = seg->permissions;
        ds->nattch = seg->tot_attached;
        ds->marked_delete = seg->marked_delete;

        release(&shm_table.lock);
        return 0;

    //IPC_SET validates the permisssion metadata for a given shmid and modifies them according to the parameters passed
    case IPC_SET:
        if (buf == 0)
            return -1;

        ds = (struct shmid_ds *)buf;

        if (ds->permissions & ~0777)
            return -1;

        acquire(&shm_table.lock);

        seg = shm_lookup(shmid, 0);

        if (seg == 0) {
            release(&shm_table.lock);
            return -1;
        }

        seg->permissions = ds->permissions & 0777;

        release(&shm_table.lock);
        return 0;

    //IPC_RMID marks the segment of given shmid for deletion by setting the marked_delete.
    case IPC_RMID:
        acquire(&shm_table.lock);

        seg = shm_lookup(shmid, 0);

        if (seg == 0) {
            release(&shm_table.lock);
            return -1;
        }

        seg->marked_delete = 1;

        if (seg->tot_attached == 0)
            shm_free(seg);

        release(&shm_table.lock);
        return 0;

    //IPC_INFO returns a structure to the user stating the current limits of the shared segment implementation.
    case IPC_INFO:
        if (buf == 0)
            return -1;

        limits = (struct shm_limits *)buf;

        limits->shmmax = SHMMAX;
        limits->shmmin = SHMMIN;
        limits->shmmni = SHM_TABLE_MAX;
        limits->max_pages_per_segment = SHM_MAX_PAGES;

        return 0;

    //SHM_INFO returns a structure to the user stating the resources of the shared segments under use currently.
    case SHM_INFO:
        if (buf == 0)
            return -1;

        used_ids = 0;
        total_pages = 0;
        total_attached = 0;

        acquire(&shm_table.lock);

        for (i = 0; i < SHM_TABLE_MAX; i++) {
            seg = &shm_table.seg[i];

            if (!seg->used)
                continue;

            used_ids++;

            total_pages +=
                (seg->shm_size + PGSIZE - 1) / PGSIZE;

            total_attached += seg->tot_attached;
        }

        release(&shm_table.lock);

        info = (struct shm_info *)buf;

        info->used_ids = used_ids;
        info->total_pages = total_pages;
        info->total_attached = total_attached;

        return 0;

    //Both find shared segment from the global shared memory table using the table index instead of shmid.
    //It then copies shared segment metadata into the structure and returns the shmid to the user.
    case SHM_STAT:
    case SHM_STAT_ANY:
        if (buf == 0)
            return -1;

        if (shmid < 0 || shmid >= SHM_TABLE_MAX)
            return -1;

        index = shmid;

        acquire(&shm_table.lock);

        seg = &shm_table.seg[index];

        if (!seg->used) {
            release(&shm_table.lock);
            return -1;
        }

        ds = (struct shmid_ds *)buf;

        ds->shmid = seg->shmid;
        ds->key = seg->key;
        ds->shm_size = seg->shm_size;
        ds->permissions = seg->permissions;
        ds->nattch = seg->tot_attached;
        ds->marked_delete = seg->marked_delete;

        index = seg->shmid;

        release(&shm_table.lock);
        return index;


    default:
        return -1;
    }
}


int
shm_fork(struct proc *parent, struct proc *child)
{
    int i, j;
    struct shmseg *seg;
    uint npages;
    uint mapped;

    //Init the child proc's shm_attachment table
    memset(child->shm_attachments, 0,
        sizeof(child->shm_attachments));

    acquire(&shm_table.lock);

    //Copy the parent's shm mapping to the child aswell.
    for (i = 0; i < SHM_MAX_ATTACHED; i++) {
        //Only valid parent attachements mapped into child.
        if (!parent->shm_attachments[i].used)
          continue;

        seg = 0;
        //search for the segment in global shm_table and if present init the seg
        for (j = 0; j < SHM_TABLE_MAX; j++) {
            if (shm_table.seg[j].used &&
              shm_table.seg[j].shmid ==
              parent->shm_attachments[i].shmid) {
                seg = &shm_table.seg[j];
                break;
            }
        }

        if (seg == 0)
          goto rollback;

        //Init the number of pages that need to be mapped to the child from that parent's shared segment.
        npages = (seg->shm_size + PGSIZE - 1) / PGSIZE;
        mapped = 0;

        //Map the parent's shared segment to the child's shm_attachments table using same VA as parent.
        for (j = 0; j < npages; j++) {
          uint va = parent->shm_attachments[i].addr
                    + j * PGSIZE;

          //Rollback if mapping not possible.
          if (mappages(child->pgdir, (void *)va,
                       PGSIZE, seg->pages[j],
                       PTE_U | PTE_W) < 0) {
            shm_unmap_pages(child->pgdir,
                            parent->shm_attachments[i].addr,
                            mapped);
            goto rollback;
          }

          mapped++;
        }

        //Finds a valid slot in child attachment table to record the entry.
        for (j = 0; j < SHM_MAX_ATTACHED; j++) {
          if (!child->shm_attachments[j].used)
            break;
        }

        //No slot available leads to exit.
        if (j == SHM_MAX_ATTACHED) {
          shm_unmap_pages(child->pgdir,
                          parent->shm_attachments[i].addr,
                          npages);
          goto rollback;
        }

        child->shm_attachments[j].used = 1;
        child->shm_attachments[j].shmid = parent->shm_attachments[i].shmid;
        child->shm_attachments[j].addr = parent->shm_attachments[i].addr;

        seg->tot_attached++;
      }

      release(&shm_table.lock);
      return 0;

    rollback:
      //Find all existing mapped segments in the child and unmap them.
      for (i = 0; i < SHM_MAX_ATTACHED; i++) {
        if (!child->shm_attachments[i].used)
          continue;

        seg = 0;

        //Search for the segment in the global shared memory table
        for (j = 0; j < SHM_TABLE_MAX; j++) {
          if (shm_table.seg[j].used &&
              shm_table.seg[j].shmid ==
              child->shm_attachments[i].shmid) {
            seg = &shm_table.seg[j];
            break;
          }
        }

        //Calculate the number of pages required to unmap and unmap them from the child.
        if (seg != 0) {
          npages = (seg->shm_size + PGSIZE - 1) / PGSIZE;

          shm_unmap_pages(child->pgdir,
                          child->shm_attachments[i].addr,
                          npages);

          if (seg->tot_attached > 0)
            seg->tot_attached--;
        }

        //Re-init the child attachment table to 0.
        memset(&child->shm_attachments[i], 0,
               sizeof(child->shm_attachments[i]));
      }

    release(&shm_table.lock);
    return -1;
}

//Detach all the shared memory mappings in exit before it deallocates all the procs physical pages.
void
shm_detach_all(struct proc *p)
{
    if (p == 0)
        return;

    shm_detach_from_pgdir(p, p->pgdir);
}

//Detach all shared memory segments from pgdir of old proc when transitioning to new one to avoid shared memory copy.
void
shm_exec_detach(struct proc *p, pde_t *oldpgdir)
{
    shm_detach_from_pgdir(p, oldpgdir);
}

