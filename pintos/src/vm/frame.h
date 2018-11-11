#ifndef VM_FRAME
#define VM_FRAME

#include <list.h>
#include "vm/page.h"
#include "threads/thread.h"
#include "threads/palloc.h"

struct ft_entry
{
  void *frame;
  struct spt_entry *spte;
  struct thread *t;
  struct list_elem elem;
};

static void ft_insert (void *, struct spt_entry *);
void *frame_alloc (enum palloc_flags);
void ft_init (void);
void *obtain_frame (enum palloc_flags, struct spt_entry *);
void free_frame (void *);

#endif