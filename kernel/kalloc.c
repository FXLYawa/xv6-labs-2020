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

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct {
  struct spinlock lock;
  int quote[PHYSTOP / PGSIZE];
} page_quo;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&page_quo.lock, "page_quo");
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

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquire(&page_quo.lock);
  if(--page_quo.quote[PA2IDX(pa)] <= 0){
  // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);

    r = (struct run*)pa;

    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
  }
  release(&page_quo.lock);
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
    memset((char*)r, 5, PGSIZE); // fill with junk
    //TODO: problem
    // acquire(&page_quo.lock);
    page_quo.quote[PA2IDX(r)] = 1;
    // release(&page_quo.lock);
  }
  return (void*)r;
}

void *
kpageclone(void *pa)
{
  acquire(&page_quo.lock);

  if(page_quo.quote[PA2IDX(pa)] <= 1){
    release(&page_quo.lock);
    return pa;
  }

  uint64 newpa = (uint64)kalloc();
  if(newpa == 0){
    release(&page_quo.lock);
    return 0;
  }

  memmove((void*)newpa, (void*)pa, PGSIZE);

  page_quo.quote[PA2IDX(pa)]--;

  release(&page_quo.lock);
  return (void*)newpa;
}

void kquoadd(void *pa)
{
  acquire(&page_quo.lock);
  page_quo.quote[PA2IDX(pa)]++;
  release(&page_quo.lock);
}