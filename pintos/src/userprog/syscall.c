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
#include "userprog/process.h"

static struct lock lock;

static int sysc = 20;
static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  lock_init(&lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/* Call shutdown. */
static int halt (void *esp) {
  power_off();
}

/* Check if stack pointer is a valid memory access.
   If yes, then extract and print the status code of the thread and exit. */
/* exit(NULL) directly terminates offending process */
int exit (void *esp) {
  int status = 0;

  if (esp != NULL) {
    sanity_check(esp);
    status = *(int *)esp;
    esp += sizeof(int);
  }
  else status = -1;
  
  struct thread *t = thread_current();
  /* Close all open files, if any */
  int i;
  for (i = 2; i < MAX_OPEN_FILES; i++) { 
    if (t->files[i] != NULL) {
      file_close(t->files[i]); 
      t->files[i] = NULL;
    }
  }
  /* Close executable file */
  if (t->executable != NULL)
  {
    file_close(t->executable);
    t->executable = NULL;
  }

  /* thread name is already correctly set in process_execute() */
  printf ("%s: exit(%d)\n", thread_current()->name, status);
  t->exit_status = status;

  sema_up(&t->sema_exit);
  sema_down(&t->sema_exit_ack);
  thread_exit ();
  return status;
}

static int exec (void *esp) { 
  /* Check if stack pointer is a valid memory access. */
  sanity_check(esp);
  /* Extract command line address and check its validity. */
  const char *cmd_line = *(char **)esp;
  esp += sizeof(char *);
  sanity_check(cmd_line);
  
  /* Create child thread from cmd_line string */
  tid_t child_tid = process_execute(cmd_line);

  /* Find child thread struct to extract its load flag */  
  struct thread *child = get_child_from_tid(child_tid);
  if (child == NULL)
    return -1;

  /* Wait on child's sema_load till its executes its loading process */
  sema_down(&child->sema_load);
  if (child->load_complete == 0) { /* Load failed check */
    child_tid = -1;
  }
  /* Allow child to proceed now that load falg has been extracted */
  sema_up (&child->sema_load_ack);
  return child_tid;
}

static int wait (void *esp) {
  /* Check if stack pointer is a valid memory access */
  sanity_check(esp);
  /* Extract pid */
  int pid = * (int *) esp;
  esp += sizeof (int);

  return process_wait(pid);
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

  // lock_acquire(&lock);
  bool success = filesys_create(file_name, (off_t)size);
  // lock_release(&lock);
  return success; 
}

/* Check if stack pointer is a valid memory access.
   If yes, then extract file name pointer and check its validity
   If valid, remove file. */
static int remove (void *esp) {
  sanity_check(esp);
  const char *file_name = *(char **)esp;
  esp += sizeof(char *);
  sanity_check(file_name);

  // lock_acquire(&lock);
  bool success = filesys_remove (file_name);
  // lock_release(&lock);
  return success;
}


/* Check if stack pointer is a valid memory access.
   If yes, then extract file name pointer and check its validity
   If valid, open file. */
static int open (void *esp) {
  sanity_check(esp);
  const char *file_name = *(char **)esp;
  esp += sizeof(char *);
  sanity_check(file_name);
  
  // lock_acquire(&lock);
  struct file* f = filesys_open(file_name);
  // lock_release(&lock);
  
  if (f == NULL)
    return -1;

  /* check the first null value in the array list of open files corresponding to the thread and make that the file fd */
  struct thread* t = thread_current();
  int i;
  for(i = 2; i < MAX_OPEN_FILES; i++)
  {
    if (t->files[i] == NULL)
    {
      t->files[i] = f;
      break;
    }
  }
  if (i == MAX_OPEN_FILES) return -1;
  else return i;
}

/* Check if stack pointer is a valid memory access.
   If yes, then extract fd find the file in the file array corresponding to the fd return its size. */
static int filesize (void *esp) {
  sanity_check(esp);
  int fd = *(int *)esp;
  esp += sizeof(int);  
  struct thread* t=thread_current();
  if (fd >= 0 && fd < MAX_OPEN_FILES && t->files[fd])
  {
    // lock_acquire(&lock);    
    int size = file_length(t->files[fd]);
    // lock_release(&lock);
    return size;
  }
  else return -1;
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
  void *buffer = *(void **)esp;
  
  /* Sanity check for buffer */
  sanity_check(buffer);
  esp += sizeof(void *);
  
  /* Extract size of buffer */
  sanity_check(esp);  
  unsigned size = *(unsigned *)esp;
  esp += sizeof(unsigned);

  sanity_check(buffer + size -1);

  struct thread* t = thread_current();
  if (fd == 0)
  {
    lock_acquire (&lock);

    int i;
    for (i = 0; i<size; i++)
      *((uint8_t *) buffer+i) = input_getc ();

    lock_release (&lock);
    return size;
  }
  else if ((fd > 1 && fd < MAX_OPEN_FILES) && t->files[fd])
  {
    lock_acquire(&lock);
    int read = file_read(t->files[fd], buffer, size);
    lock_release(&lock);
    return (int) read;
  }
  else return -1;
}

/* Check if stack pointer is a valid memory access.
   If yes, then extract fd to find the file in the file array corresponding to the fd 
   Extract  the buffer validate it and extract size then write file */
static int write (void *esp) {
  /* Extract fd */
  sanity_check(esp);
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
  /* Print on console if fd is equal to 1 */
  if (fd == 1) {
    lock_acquire(&lock);
    putbuf(buffer, size);
    lock_release(&lock);
    return (int) size;
    // lock_acquire (&lock);

    // int i;
    // for (i = 0; i<size; i++)
    //   putchar (*((char *) buffer + i));

    // lock_release (&lock);
    // return i;
  }
  else if ((fd > 1 && fd < MAX_OPEN_FILES) && t->files[fd])
  {
    lock_acquire(&lock);
    int write = file_write(t->files[fd], buffer, (off_t) size);
    lock_release(&lock);
    return write;
  }
  return 0;
}

/* Check if stack pointer is a valid memory access.
   If yes, then extract fd to find the file in the file array corresponding to the fd 
   Extract  the position then seek in the file */
static int seek (void *esp) {
  /* Extract fd */
  sanity_check(esp);  
  int fd = *(int *)esp;
  esp += sizeof(int);

  /* Extract position */
  sanity_check(esp);  
  unsigned position = *(unsigned *)esp;
  esp += sizeof(unsigned);

  struct thread* t = thread_current();

  /*check among valid fd*/
  if((fd > 1 && fd < MAX_OPEN_FILES) && t->files[fd])
  {
    // lock_acquire(&lock);
    file_seek(t->files[fd], (off_t)position);
    // lock_release(&lock);
  } 
}


/* Check if stack pointer is a valid memory access.
   If yes, then extract fd to find the file in the file array corresponding to the fd then tell in the file */
static int tell (void *esp) {
  /* Extract fd */
  sanity_check(esp);
  int fd = *(int *)esp;
  esp += sizeof(int);
  
  struct thread* t = thread_current();

  /*check among valid fd*/
  if ((fd > 1 && fd < MAX_OPEN_FILES) && t->files[fd])
  {
    // lock_acquire(&lock);
    int pos = file_tell(t->files[fd]);
    // lock_release(&lock);
    return pos;
  }
  else return -1;
}

/* Check if stack pointer is a valid memory access.
   If yes, then extract fd to find the file in the file array corresponding to the fd then close the file */
static int close (void *esp) {
  /* Extract fd */
  sanity_check(esp);
  int fd = *(int *)esp;
  esp += sizeof(int);

  struct thread *t = thread_current();

  /*check among valid fd*/
  if((fd > 1 && fd < MAX_OPEN_FILES) && t->files[fd])
  {
    // lock_acquire(&lock);
    file_close(t->files[fd]);
    t->files[fd] = NULL;
    // lock_release(&lock);
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
    // printf ("%s: exit(%d)\n", thread_current()->name, -1);
    // thread_exit ();
    exit(NULL);
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
    exit(NULL);
  }
}
