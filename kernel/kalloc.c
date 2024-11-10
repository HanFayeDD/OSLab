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

struct run
{
  struct run *next;
};

// 在使用kalloc()获取内存时，由于添加了内存锁kmem.lock，
// 其他CPU如果要切换进行内存申请必须等待当前进程释放内存锁
struct kmem
{
  struct spinlock lock;
  struct run *freelist; // 空闲物理内存页组成的链表每个页帧4KB
};

struct kmem kmems[NCPU];

void kinit()
{
  // char kmems_name[8][20];
  // for循环依次初始化八个cpu对应的kmen数据结构
  for (int i = 0; i < NCPU; i++)
  {
    // snprintf(kmems_name[i], sizeof(kmems_name[0]), "kmems_%d", i);
    initlock(&kmems[i].lock, "kmem");
  }
  freerange(end, (void *)PHYSTOP);
}

void kfreeinit(void *pa, int i)
{
  struct run *r;
  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");
  memset(pa, 1, PGSIZE);
  r = (struct run *)pa;
  acquire(&kmems[i].lock);
  r->next = kmems[i].freelist; // 链表采用头插方法
  kmems[i].freelist = r;
  release(&kmems[i].lock);
}

void freerange(void *pa_start, void *pa_end)
{
  char *p;
  // int i = 0;
  p = (char *)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
  {
    // kfreeinit(p, i);
    // i = (i + 1) % NCPU;
    kfree(p);
  }
}


// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void *pa)
{
  struct run *r;
  push_off();
  int i = cpuid();
  pop_off();
  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run *)pa;

  acquire(&kmems[i].lock);
  r->next = kmems[i].freelist; // 链表采用头插方法
  kmems[i].freelist = r;
  release(&kmems[i].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  struct run *temp;

  push_off();
  int i = cpuid();
  pop_off();
  acquire(&kmems[i].lock);
  r = kmems[i].freelist;
  if (r)
  { // 在自己里找到
    kmems[i].freelist = r->next;
    // memset((char *)r, 5, PGSIZE);
    release(&kmems[i].lock);
    memset((char *)r, 5, PGSIZE);
    return (void *)r;
  }

  release(&kmems[i].lock);

  // 没找到
  for (int j = 0; j < NCPU; j++)
  {
    if (j == i)
    { // 自己本来就没用
      continue;
    }
    acquire(&kmems[j].lock);
    temp = kmems[j].freelist;
    if (!temp)
    { // j也没用T T
      release(&kmems[j].lock);
      continue;
    }
    else
    { // j有用
      kmems[j].freelist = temp->next;
      r = temp;
      release(&kmems[j].lock);
      memset((char *)r, 5, PGSIZE);
      break;
    }
  }
  
  return (void *)r;

}
