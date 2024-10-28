#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64 sys_yield(void){
  struct proc *p = myproc();
  // 打印进程的内核线程上下文被保存的地址范围
  struct context *p_context = &p->context;
  struct context *endPtr = (struct context *)((char *)p_context + sizeof(struct context));
  printf("Save the context of the process to the memory region from address %p to %p\n", (void *)p_context, (void *)endPtr);
  // 打印pid和pc
  printf("Current running process pid is %d and user pc is %p\n", p->pid, p->trapframe->epc);
  //找到下一个runnable
  for(p = proc; p < &proc[NPROC]; p++){
    acquire(&p->lock);
    if (p->state == RUNNABLE){
      printf("Next runnable process pid is %d and user pc is %p\n", p->pid, p->trapframe->epc);
      release(&p->lock);
      break;
    }
    release(&p->lock);
  }
  //挂起
  yield();
  // printf("Next runnable process pid is %d and user pc is %p\n", p->pid, p->trapframe->epc);
  return 0;
}


uint64 sys_exit(void) {
  int n;
  if (argint(0, &n) < 0) return -1;
  exit(n);
  return 0;  // not reached
}

uint64 sys_getpid(void) { return myproc()->pid; }

uint64 sys_fork(void) { return fork(); }

uint64 sys_wait(void) {
  uint64 p;
  int q;
  if (argaddr(0, &p) < 0) return -1;
  if (argint(1, &q) < 0) return -1;
  return wait(p, q);
}

uint64 sys_sbrk(void) {
  int addr;
  int n;

  if (argint(0, &n) < 0) return -1;
  addr = myproc()->sz;
  if (growproc(n) < 0) return -1;
  return addr;
}

uint64 sys_sleep(void) {
  int n;
  uint ticks0;

  if (argint(0, &n) < 0) return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n) {
    if (myproc()->killed) {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64 sys_kill(void) {
  int pid;

  if (argint(0, &pid) < 0) return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64 sys_uptime(void) {
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64 sys_rename(void) {
  char name[16];
  int len = argstr(0, name, MAXPATH);
  if (len < 0) {
    return -1;
  }
  struct proc *p = myproc();
  memmove(p->name, name, len);
  p->name[len] = '\0';
  return 0;
}
