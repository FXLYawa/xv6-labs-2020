// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define BUCKET_CNT 13
#define hash(blockno) ((blockno) % BUCKET_CNT)
extern uint ticks;


struct {
  struct spinlock lock;
  struct buf buf[NBUF];
  int size;
  struct buf buckets[BUCKET_CNT];
  struct spinlock locks[BUCKET_CNT];
  struct spinlock hashlock;

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  // struct buf head;
} bcache;

void
binit(void)
{
  struct buf *b;
  bcache.size = 0;
  initlock(&bcache.lock, "bcache");
  initlock(&bcache.hashlock, "bchash");
  for(int i = 0; i < BUCKET_CNT; i++) 
    initlock(&bcache.locks[i], "bclock");

  for(b = bcache.buf; b < bcache.buf + NBUF; b++)
    initsleeplock(&b->lock, "buffer");
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int id = hash(blockno);

  acquire(&bcache.locks[id]);
  for(b = bcache.buckets[id].next; b; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bcache.locks[id]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  acquire(&bcache.lock);
  if(bcache.size < NBUF) {
    b = &bcache.buf[bcache.size++];
    b->dev = dev;
    b->blockno = blockno;
    b->valid = 0;
    b->refcnt = 1;
    b->next = bcache.buckets[id].next;
    bcache.buckets[id].next = b;
    release(&bcache.lock);
    release(&bcache.locks[id]);
    acquiresleep(&b->lock);
    return b;
  }
  release(&bcache.lock);
  release(&bcache.locks[id]);

  struct buf *pre, *minb = 0, *minpre;
  uint min_tamp;
  acquire(&bcache.hashlock);
  for(int i = 0; i < BUCKET_CNT; i++) {
    min_tamp = -1;
    acquire(&bcache.locks[id]);
    for(pre = &bcache.buckets[id], b = pre->next; b; pre = b, b = b->next) {
      if(id == hash(blockno) && b->dev == dev && b->blockno == blockno) {
        b->refcnt++;
        release(&bcache.locks[id]);
        release(&bcache.hashlock);
        acquiresleep(&b->lock);
        return b;
      }
      if(b->refcnt == 0 && b->timestamp < min_tamp) {
        min_tamp = b->timestamp;
        minb = b;
        minpre = pre;
      }
    }

    if(minb) {
      minb->dev = dev;
      minb->blockno = blockno;
      minb->valid = 0;
      minb->refcnt = 1;
      if(id != hash(blockno)) {
        minpre->next = minb->next;
        release(&bcache.locks[id]);
        id = hash(blockno);
        acquire(&bcache.locks[id]);
        minb->next = bcache.buckets[id].next;
        bcache.buckets[id].next = minb;
      }
      release(&bcache.locks[id]);
      release(&bcache.hashlock);
      acquiresleep(&minb->lock);
      return minb;
    }
    release(&bcache.locks[id]);
    if(++id == BUCKET_CNT) id = 0;
  }

  // // Is the block already cached?
  // for(b = bcache.head.next; b != &bcache.head; b = b->next){
  //   if(b->dev == dev && b->blockno == blockno){
  //     b->refcnt++;
  //     release(&bcache.lock);
  //     acquiresleep(&b->lock);
  //     return b;
  //   }
  // }

  // // Not cached.
  // // Recycle the least recently used (LRU) unused buffer.
  // for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
  //   if(b->refcnt == 0) {
  //     b->dev = dev;
  //     b->blockno = blockno;
  //     b->valid = 0;
  //     b->refcnt = 1;
  //     release(&bcache.lock);
  //     acquiresleep(&b->lock);
  //     return b;
  //   }
  // }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int id = hash(b->blockno);
  acquire(&bcache.locks[id]);
  b->refcnt--;
  if(b->refcnt == 0)
    b->timestamp = ticks;
  release(&bcache.locks[id]);

  // acquire(&bcache.lock);
  // b->refcnt--;
  // if (b->refcnt == 0) {
  //   // no one is waiting for it.
  //   b->next->prev = b->prev;
  //   b->prev->next = b->next;
  //   b->next = bcache.head.next;
  //   b->prev = &bcache.head;
  //   bcache.head.next->prev = b;
  //   bcache.head.next = b;
  // }
  
  // release(&bcache.lock);
}

void
bpin(struct buf *b) {
  int id = hash(b->blockno);
  acquire(&bcache.locks[id]);
  b->refcnt++;
  release(&bcache.locks[id]);
}

void
bunpin(struct buf *b) {
  int id = hash(b->blockno);
  acquire(&bcache.locks[id]);
  b->refcnt--;
  release(&bcache.locks[id]);
}


