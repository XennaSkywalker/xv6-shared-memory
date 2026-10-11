#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "shm.h"

struct shm_table shm_table;
static int next_shmid = 1;

//shmget() acquires the lock.
//shm_find_by_key() and shm_find_by_shmid() assume the lock is already held.
//shm_create() assumes the lock is already held.
//shm_free() assumes the lock is held when modifying a segment in the global table.

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

    if (size == 0)
        return -1;

    //size exceeds possible size of a segment
    if (size / PGSIZE + (size % PGSIZE != 0) > SHM_MAX_PAGES)
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

    if (size == 0)
        return -1;

    if ((size + PGSIZE - 1)/PGSIZE > SHM_MAX_PAGES)
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
shm_at(int shmid)
{
    struct proc *p;
    struct shmseg *seg;
    struct shmseg *other;
    uint addr;
    uint npages;
    uint other_addr;
    uint other_npages;
    uint i;
    uint j;
    int slot;
    int overlap;
    int found;

    p = myproc();

    if (p == 0) return -1;

    acquire(&shm_table.lock);

    seg = 0;

    for (i = 0; i < SHM_TABLE_MAX; i++) {
        if (shm_table.seg[i].used &&
            shm_table.seg[i].shmid == shmid &&
            !shm_table.seg[i].marked_delete) {
        seg = &shm_table.seg[i];
        break;
        }
    }

    if (seg == 0) {
        release(&shm_table.lock);
        return -1;
    }

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

    npages = (seg->shm_size + PGSIZE - 1) / PGSIZE;

    if (npages == 0 || npages > SHM_MAX_PAGES) {
        release(&shm_table.lock);
        return -1;
    }

    addr = SHM_BASE;
    found = 0;

    while (addr < KERNBASE &&
            npages <= (KERNBASE - addr) / PGSIZE) {
        overlap = 0;

    /*
     * Check overlap with the process's ordinary address space.
     */
        if (addr < p->sz &&
            p->sz - addr >= PGSIZE) {
        overlap = 1;
        }

    /*
     * Check overlap with existing shared-memory attachments.
     */
        for (i = 0; i < SHM_MAX_ATTACHED && !overlap; i++) {
            if (!p->shm_attachments[i].used) continue;

            other = 0;

            for (j = 0; j < SHM_TABLE_MAX; j++) {
                if (shm_table.seg[j].used &&
                    shm_table.seg[j].shmid ==
                    p->shm_attachments[i].shmid) {
                    other = &shm_table.seg[j];
                    break;
                    }
            }

            if (other == 0) continue;

            other_addr = p->shm_attachments[i].addr;
            other_npages = (other->shm_size + PGSIZE - 1) / PGSIZE;

            if (addr < other_addr + other_npages * PGSIZE &&
                other_addr < addr + npages * PGSIZE) {
                overlap = 1;
            }
        }

    /*
     * Check actual page-table entries.
     */
        if (!overlap &&
            shm_range_free(p->pgdir, addr, npages)) {
            found = 1;
            break;
        }

    /*
     * Advance to the next page-aligned candidate.
     */
        if (addr > KERNBASE - PGSIZE) break;

        addr += PGSIZE;
    }

    if (!found) {
        release(&shm_table.lock);
        return -1;
    }

  /*
   * Map the segment's physical pages into this process.
   */
    for (i = 0; i < npages; i++) {
        if (mappages(p->pgdir,
                 (void *)(addr + i * PGSIZE),
                 PGSIZE,
                 seg->pages[i],
                 PTE_U | PTE_W) < 0) {
            shm_unmap_pages(p->pgdir, addr, i);
            release(&shm_table.lock);
            return -1;
        }
    }

  /*
   * Record the attachment only after all mappings succeed.
   */
    p->shm_attachments[slot].used = 1;
    p->shm_attachments[slot].shmid = shmid;
    p->shm_attachments[slot].addr = addr;

    seg->tot_attached++;

    release(&shm_table.lock);

    return (int)addr;
}

int
shm_fork(struct proc *parent, struct proc *child)
{
    int i, j;
      struct shmseg *seg;
      uint npages;
      uint mapped;

      memset(child->shm_attachments, 0,
             sizeof(child->shm_attachments));

      acquire(&shm_table.lock);

      for (i = 0; i < SHM_MAX_ATTACHED; i++) {
        if (!parent->shm_attachments[i].used)
          continue;

        seg = 0;

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

        npages = (seg->shm_size + PGSIZE - 1) / PGSIZE;
        mapped = 0;

        for (j = 0; j < npages; j++) {
          uint va = parent->shm_attachments[i].addr
                    + j * PGSIZE;

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

        for (j = 0; j < SHM_MAX_ATTACHED; j++) {
          if (!child->shm_attachments[j].used)
            break;
        }

        if (j == SHM_MAX_ATTACHED) {
          shm_unmap_pages(child->pgdir,
                          parent->shm_attachments[i].addr,
                          npages);
          goto rollback;
        }

        child->shm_attachments[j].used = 1;
        child->shm_attachments[j].shmid =
            parent->shm_attachments[i].shmid;
        child->shm_attachments[j].addr =
            parent->shm_attachments[i].addr;

        seg->tot_attached++;
      }

      release(&shm_table.lock);
      return 0;

    rollback:
      for (i = 0; i < SHM_MAX_ATTACHED; i++) {
        if (!child->shm_attachments[i].used)
          continue;

        seg = 0;

        for (j = 0; j < SHM_TABLE_MAX; j++) {
          if (shm_table.seg[j].used &&
              shm_table.seg[j].shmid ==
              child->shm_attachments[i].shmid) {
            seg = &shm_table.seg[j];
            break;
          }
        }

        if (seg != 0) {
          npages = (seg->shm_size + PGSIZE - 1) / PGSIZE;

          shm_unmap_pages(child->pgdir,
                          child->shm_attachments[i].addr,
                          npages);

          if (seg->tot_attached > 0)
            seg->tot_attached--;
        }

        memset(&child->shm_attachments[i], 0,
               sizeof(child->shm_attachments[i]));
      }

    release(&shm_table.lock);
    return -1;
}

void
shm_detach_all(struct proc *p)
{
    if (p == 0)
        return;

    shm_detach_from_pgdir(p, p->pgdir);
}

void
shm_exec_detach(struct proc *p, pde_t *oldpgdir)
{
    shm_detach_from_pgdir(p, oldpgdir);
}

