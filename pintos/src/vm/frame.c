#include "threads/synch.h"
#include "threads/palloc.h"
#include "vm/frame.h"
#include "vm/page.h"
#include <malloc.h>

struct list ft;
struct lock ft_lock;

void
ft_init (void)
{
  list_init (&ft);
  lock_init (&ft_lock);
}

void *
obtain_frame (enum palloc_flags flags, struct spt_entry *entry)
{
  if (flags | PAL_USER != 0)
  {
    void *frame = frame_alloc (flags);
    if (frame != NULL && entry != NULL){
      ft_insert (frame, entry);
      return frame;
    }
  }
  return NULL;
}

static void
ft_insert (void *frame, struct spt_entry *entry) {
  struct ft_entry *fte = (struct ft_entry *) malloc (sizeof (struct ft_entry));
  fte->frame = frame;
  fte->spte = entry;
  fte->t = thread_current ();
  lock_acquire (&ft_lock);
  list_push_back (&ft, &entry->elem);
  lock_release (&ft_lock);
}

void *
frame_alloc (enum palloc_flags flags)
{
  if (flags | PAL_USER != 0)
  {
    void *frame = palloc_get_page (flags);
    if (frame != NULL)
      return frame;
  }
  return NULL;
}

void
free_frame (void *frame)
{
  palloc_free_page (frame);
}
