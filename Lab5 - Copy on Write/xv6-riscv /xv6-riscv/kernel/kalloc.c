// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.
#define PA2IDX(pa) (((uint64)(pa) - KERNBASE) >> 12)
#define MAX_PHYS_PAGES ((PHYSTOP - KERNBASE) / PGSIZE)
static int refcnt[MAX_PHYS_PAGES];
struct spinlock ref_lock;

void
incref(uint64 pa) {
  acquire(&ref_lock);
  int idx = PA2IDX(pa);
  if(idx < 0 || idx >= (sizeof(refcnt)/sizeof(refcnt[0])))
    panic("incref: bad pa");
  refcnt[idx]++;
  release(&ref_lock);
}

void
decref(uint64 pa) {
  acquire(&ref_lock);
  int idx = PA2IDX(pa);
  if(idx < 0 || idx >= (sizeof(refcnt)/sizeof(refcnt[0])))
    panic("decref: bad pa");
  if(refcnt[idx] <= 0)
    panic("decref: already zero");
  refcnt[idx]--;
  release(&ref_lock);
}

int
getref(uint64 pa) {
  acquire(&ref_lock);
  int idx = PA2IDX(pa);
  if(idx < 0 || idx >= (sizeof(refcnt)/sizeof(refcnt[0])))
    panic("getref: bad pa");
  int r = refcnt[idx];
  release(&ref_lock);
  return r;
}


struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&ref_lock, "ref_lock");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)

void
kfree(void *pa)
{
  struct run *r;
  uint64 pa64 = (uint64)pa;
  int idx = PA2IDX(pa64);

  if((pa64 % PGSIZE) != 0 || (char*)pa < end || pa64 >= PHYSTOP)
    panic("kfree");

  acquire(&ref_lock);


  refcnt[idx]--;
  int count = refcnt[idx];
  release(&ref_lock);

  if(count > 0){
    // still being used by another process → do not free
    return;
  }

  // actual free: no process is using this page anymore
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;
  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.

void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r){
    memset((char*)r, 5, PGSIZE); // fill with junk for debugging

    uint64 pa = (uint64)r;
    int idx = PA2IDX(pa);

    acquire(&ref_lock);
    refcnt[idx] = 1;   // brand new page, exactly 1 reference
    release(&ref_lock);
  }

  return (void*)r;
}
