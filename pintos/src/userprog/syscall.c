#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/init.h"
#include "lib/kernel/console.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"

static int sysc = 20;
static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static int halt (void *esp) {
  power_off();
}

int exit (void *esp) {
  sanity_check(esp);
  int status = *(int *)esp;
  esp += sizeof(int);
  printf ("%s: exit(%d)\n", thread_current()->name, status);
  thread_exit ();
  return status;
}

static int exec (void *esp) { 
  printf ("system call!\n");
  thread_exit ();
}

static int wait (void *esp) {
  printf ("system call!\n");
  thread_exit ();
}

static int create (void *esp) {
  printf ("system call!\n");
  thread_exit ();
}

static int remove (void *esp) {
  printf ("system call!\n");
  thread_exit ();
}

static int open (void *esp) {
  printf ("system call!\n");
  thread_exit ();
}

static int filesize (void *esp) {
  printf ("system call!\n");
  thread_exit ();
}

static int read (void *esp) {
  printf ("system call!\n");
  thread_exit ();
}

static int write (void *esp) {
  // printf("%s\n", "writing");
  sanity_check(esp);
  int fd = *(int *)esp;
  esp += sizeof(int);
  // printf("%d\n", fd);
  sanity_check(esp);
  const void *buffer = *(void **)esp;
  sanity_check(buffer);
  // char *buffer = *((char **) esp);
  esp += sizeof(char *);
  // printf("%s\n", buffer);
  sanity_check(esp);  
  unsigned size = *(unsigned *)esp;
  esp += sizeof(unsigned);
  // printf("%d\n", size);

  if (fd == 1) {
    putbuf(buffer, size);
    return (int) size;
  }
  return 0;
}

static int seek (void *esp) {
  printf ("system call!\n");
  thread_exit ();
}

static int tell (void *esp) {
  printf ("system call!\n");
  thread_exit ();
}

static int close (void *esp) {
  printf ("system call!\n");
  thread_exit ();
}

static int
mmap (void *esp)
{
  thread_exit ();
}

static int
munmap (void *esp)
{
  thread_exit ();
}

static int
chdir (void *esp)
{
  thread_exit ();
}

static int
mkdir (void *esp)
{
  thread_exit ();
}

static int
readdir (void *esp)
{
  thread_exit ();
}

static int
isdir (void *esp)
{
  thread_exit ();
}

static int
inumber (void *esp)
{
  thread_exit ();
}

static int (*syscalls []) (void *) = {
  halt,
  exit,
  exec,
  wait,
  create,
  remove,
  open,
  filesize,
  read,
  write,
  seek,
  tell,
  close,

  mmap,
  munmap,

  chdir,
  mkdir,
  readdir,
  isdir,
  inumber
};

void sanity_check(void *esp) {
  if (esp == NULL || is_kernel_vaddr(esp) || pagedir_get_page(thread_current()->pagedir, esp) == NULL) {
    printf ("%s: exit(%d)\n", thread_current()->name, -1);
    thread_exit ();
  }
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  void *esp = f->esp;
  /* check for invalid esp, terminate offending thread on detection */
  sanity_check(esp);
  int syscallNum = *(int *)esp;
  esp += sizeof(int);
  if (syscallNum >= 0 && syscallNum < sysc) {
    /* Valid */
    int (*function) (void *) = syscalls[syscallNum];
    f->eax = function(esp);
  }
  else {
    /* Invalid */
    printf ("%s: exit(%d)\n", thread_current()->name, -1);
    thread_exit ();
  }
}
