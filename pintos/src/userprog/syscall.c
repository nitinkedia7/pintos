#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/init.h"
#include "lib/kernel/console.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/synch.h"

static int sysc = 20;
static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* Call shutdown. */
static int halt (void *esp) {
  power_off();
}

/* Check if stack pointer is a valid memory access.
   If yes, then extract and print the status code of the thread and exit. */
int exit (void *esp) {
  sanity_check(esp);
  int status = *(int *)esp;
  esp += sizeof(int);
  printf ("%s: exit(%d)\n", thread_current()->name, status);
  thread_exit ();
  return status;
}

/* Check if stack pointer is a valid memory access.
   If yes, then extract command line address and check its validity. */
static int exec (void *esp) { 
  sanity_check(esp);
  const char *cmd_line = *(char **)esp;
  esp += sizeof(char *);
  sanity_check(cmd_line);
}

/* Check if stack pointer is a valid memory access.
   If yes, exit thread */
static int wait (void *esp) {
  sanity_check(esp);
  printf ("%s: exit(%d)\n", thread_current()->name, -1);
  thread_exit();
}

/* Check if stack pointer is a valid memory access.
   If yes, then extract file name pointer and check its validity
   If valid, extract size and call function to create file. */
static int create (void *esp) {
  sanity_check(esp);
  const char *file_name = *(char **)esp;
  esp += sizeof(char *);
  sanity_check(file_name);

  sanity_check(esp);
  unsigned size = *(unsigned *)esp;
  esp += sizeof(unsigned);

  return filesys_create(file_name, (off_t)size); 
}

/* Check if stack pointer is a valid memory access.
   If yes, then extract file name pointer and check its validity
   If valid, remove file. */
static int remove (void *esp) {
  sanity_check(esp);
  const char *file_name = *(char **)esp;
  esp += sizeof(char *);
  sanity_check(file_name);

  return filesys_remove (file_name);
}


/* Check if stack pointer is a valid memory access.
   If yes, then extract file name pointer and check its validity
   If valid, open file. */
static int open (void *esp) {
  sanity_check(esp);
  const char *file_name = *(char **)esp;
  esp += sizeof(char *);
  sanity_check(file_name);
  struct file* f = filesys_open(file_name);
  if(f == NULL)
  {
    return -1;
  }

  /* check the first null value in the array list of open files corresponding to the thread and make that the file fd */
  struct thread* t = thread_current();
  int i;
  for(i=2; i<MAX_OPEN_FILES; i++)
  {
    if(t->files[i] == NULL)
    {
      t->files[i] = f;
      break;
    }
  }
  return i;
 
}

/* Check if stack pointer is a valid memory access.
   If yes, then extract fd find the file in the file array corresponding to the fd return its size. */
static int filesize (void *esp) {
  sanity_check(esp);
  int fd = *(int *)esp;
  esp += sizeof(int);  
  struct thread* t=thread_current();
  if(t->files[fd])
  {
    return file_length(t->files[fd]);
  }
}

/* Check if stack pointer is a valid memory access.
   If yes, then extract fd to find the file in the file array corresponding to the fd 
   Extract  the buffer validate it and extract size then read file */
static int read (void *esp) {
  sanity_check(esp);

/* Extract fd */
  int fd = *(int *)esp;
  esp += sizeof(int);

/* Extract buffer */
  sanity_check(esp);
  const void *buffer = *(void **)esp;
  
/* Sanity check for buffer */
  sanity_check(buffer);
  esp += sizeof(char *);
  
/* Extract size of buffer */
  sanity_check(esp);  
  unsigned size = *(unsigned *)esp;
  esp += sizeof(unsigned);

  struct thread* t = thread_current();

  /*check among valid fd*/
  if((fd > 1 && fd <= MAX_OPEN_FILES) && t->files[fd])
  {
    return file_read(t->files[fd], buffer, (off_t)size);
  }
}

/* Check if stack pointer is a valid memory access.
   If yes, then extract fd to find the file in the file array corresponding to the fd 
   Extract  the buffer validate it and extract size then write file */
static int write (void *esp) {
  
  sanity_check(esp);

/* Extract fd */
  int fd = *(int *)esp;
  esp += sizeof(int);

/* Extract buffer */
  sanity_check(esp);
  const void *buffer = *(void **)esp;
  
/* Sanity check for buffer */
  sanity_check(buffer);
  esp += sizeof(char *);
  
/* Extract size of buffer */
  sanity_check(esp);  
  unsigned size = *(unsigned *)esp;
  esp += sizeof(unsigned);

/* Print on console if fd is equal to 1 */
  if (fd == 1) {
    putbuf(buffer, size);
    return (int) size;
  }

  struct thread* t = thread_current();

   /*check among valid fd*/
  if((fd > 1 && fd <= MAX_OPEN_FILES) && t->files[fd])
  {
    return file_write(t->files[fd], buffer, (off_t)size);
  }
  return 0;
}

/* Check if stack pointer is a valid memory access.
   If yes, then extract fd to find the file in the file array corresponding to the fd 
   Extract  the position then seek in the file */
static int seek (void *esp) {
   sanity_check(esp);

/* Extract fd */
  int fd = *(int *)esp;
  esp += sizeof(int);

  sanity_check(esp);  
  unsigned position = *(unsigned *)esp;
  esp += sizeof(unsigned);

  struct thread* t = thread_current();

  /*check among valid fd*/
  if((fd > 1 && fd <= MAX_OPEN_FILES) && t->files[fd])
  {
    file_seek(t->files[fd], (off_t)position);
  } 
}


/* Check if stack pointer is a valid memory access.
   If yes, then extract fd to find the file in the file array corresponding to the fd then tell in the file */
static int tell (void *esp) {
  sanity_check(esp);

/* Extract fd */
  int fd = *(int *)esp;
  esp += sizeof(int);
  
  struct thread* t = thread_current();

  /*check among valid fd*/
  if((fd > 1 && fd <= MAX_OPEN_FILES) && t->files[fd])
  {
    return file_tell(t->files[fd]);
  }
}

/* Check if stack pointer is a valid memory access.
   If yes, then extract fd to find the file in the file array corresponding to the fd then close the file */
static int close (void *esp) {
  sanity_check(esp);

/* Extract fd */
  int fd = *(int *)esp;
  esp += sizeof(int);

  struct thread *t = thread_current();

  /*check among valid fd*/
  if((fd > 1 && fd <= MAX_OPEN_FILES) && t->files[fd])
  {
    file_close(t->files[fd]);
    t->files[fd] = NULL;
  }
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

/* Static array of functions. */
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

/* Check if the stack pointer is not pointing to null, kernel space and there is no access to unmapped virtual memory.*/
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
