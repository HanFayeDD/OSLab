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
#define NBUCKETS 13

// struct {
//   struct spinlock lock;
//   struct buf buf[NBUF];

//   // Linked list of all buffers, through prev/next.
//   // Sorted by how recently the buffer was used.
//   // head.next is most recent, head.prev is least.
//   struct buf head;
// } bcache;

struct
{
  struct spinlock lock[NBUCKETS];
  struct spinlock gloablock;
  struct buf buf[NBUF];
  // Linked list of all buffers, through prev/next.
  // head.next is most recently used.
  // struct buf head;
  struct buf hashbucket[NBUCKETS]; // 每个哈希队列一个linked list及一个lock
} bcache;

void binit(void)
{
  struct buf *b;
  // initlock(&bcache.lock, "bcache");
  for (int i = 0; i < NBUCKETS; i++)
  {
    initlock(&bcache.lock[i], "bcache");
  }
  initlock(&bcache.gloablock, "bcache");

  // Create linked list of buffers
  // bcache.head.prev = &bcache.head;
  // bcache.head.next = &bcache.head;
  for (int i = 0; i < NBUCKETS; i++)
  {
    bcache.hashbucket[i].prev = &bcache.hashbucket[i];
    bcache.hashbucket[i].next = &bcache.hashbucket[i];
  }

  int cnt = 0;
  for (b = bcache.buf; b < bcache.buf + NBUF; b++)
  {
    // b->next = bcache.head.next;
    // b->prev = &bcache.head;
    // initsleeplock(&b->lock, "buffer");
    // bcache.head.next->prev = b;
    // bcache.head.next = b; //相当于在头部插入了b且是一个类似双向链表的结构（循环链表）
    b->next = bcache.hashbucket[cnt].next;
    b->prev = &bcache.hashbucket[cnt];
    initsleeplock(&b->lock, "buffer");
    bcache.hashbucket[cnt].next->prev = b;
    bcache.hashbucket[cnt].next = b;
    cnt = (cnt + 1) % NBUCKETS;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf *
bget(uint dev, uint blockno)
{
  struct buf *b;
  int hashnum = blockno % NBUCKETS;
  acquire(&bcache.lock[hashnum]);
  // Is the block already cached?
  for (b = bcache.hashbucket[hashnum].next; b != &bcache.hashbucket[hashnum]; b = b->next)
  { // 从循环链表的前面往后面找
    if (b->dev == dev && b->blockno == blockno)
    { // 命中
      b->refcnt++;
      release(&bcache.lock[hashnum]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for (b = bcache.hashbucket[hashnum].prev; b != &bcache.hashbucket[hashnum]; b = b->prev)
  { // 从循环链表的后面往前找
    if (b->refcnt == 0)
    { // 找到一个最不经常使用且引用为0的block
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock[hashnum]);
      acquiresleep(&b->lock); // 获得对缓冲区的锁
      return b;
    }
  }

  // 1.找不到以后释放锁,至此函数不持有任何锁
  release(&bcache.lock[hashnum]);
  // 2.1获取全局锁
  acquire(&bcache.gloablock);
  // 2.2获取哈希桶的锁
  acquire(&bcache.lock[hashnum]);
  // 2.3是否有已经缓存
  for (b = bcache.hashbucket[hashnum].next; b != &bcache.hashbucket[hashnum]; b = b->next)
  {
    if (b->dev == dev && b->blockno == blockno)
    {
      b->refcnt++;
      release(&bcache.lock[hashnum]);
      release(&bcache.gloablock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  // 2.4该哈希桶有无空闲
  for (b = bcache.hashbucket[hashnum].prev; b != &bcache.hashbucket[hashnum]; b = b->prev)
  {
    if (b->refcnt == 0)
    {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock[hashnum]);
      release(&bcache.gloablock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  // 3.要从其他桶内查找可用空闲块并分配
  for (int i = 0; i < NBUCKETS; i++)
  {
    if (i == hashnum)
    {
      continue;
    }
    acquire(&bcache.lock[i]);
    for (b = bcache.hashbucket[i].prev; b != &bcache.hashbucket[i]; b = b->prev)
    {
      if (b->refcnt == 0)
      {
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        release(&bcache.lock[i]);
        release(&bcache.lock[hashnum]);
        release(&bcache.gloablock);
        acquiresleep(&b->lock);
        return b;
      }
    }
    release(&bcache.lock[i]);
  }

  // 最后：返回所有锁
  release(&bcache.lock[hashnum]);
  release(&bcache.gloablock);

  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf *
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if (!b->valid)
  {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void bwrite(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void brelse(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("brelse");

  int bufnum = b->blockno;
  int hashnum = bufnum % NBUCKETS;

  releasesleep(&b->lock);

  acquire(&bcache.lock[hashnum]);
  b->refcnt--;
  if (b->refcnt == 0)
  {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.hashbucket[hashnum].next; // 归还的block插入在双向链表头部
    b->prev = &bcache.hashbucket[hashnum];
    bcache.hashbucket[hashnum].next->prev = b;
    bcache.hashbucket[hashnum].next = b;
  }

  release(&bcache.lock[hashnum]);
}

void bpin(struct buf *b)
{
  int bnum = b->blockno;
  int hashnum = bnum % NBUCKETS;
  acquire(&bcache.lock[hashnum]);
  b->refcnt++;
  release(&bcache.lock[hashnum]);
}

void bunpin(struct buf *b)
{
  int bnum = b->blockno;
  int hashnum = bnum % NBUCKETS;
  acquire(&bcache.lock[hashnum]);
  b->refcnt--;
  release(&bcache.lock[hashnum]);
}
