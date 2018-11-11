#include "vm/page.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/palloc.h"

struct hash spt;
struct lock spt_lock;


unsigned
spt_hash_func (const struct hash_elem *element, void *aux)
{
  struct spt_entry *entry = hash_entry (element, struct spt_entry, elem);
  return hash_int ((int) entry->user_page);
}


bool
spt_less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux)
{
  struct spt_entry *entry_a = hash_entry (a, struct spt_entry, elem);
  struct spt_entry *entry_b = hash_entry (b, struct spt_entry, elem);
  return ((int) entry_a->user_page) < ((int) entry_b->user_page);
}


void
spt_init ()
{
  hash_init (&spt, spt_hash_func, spt_less_func, NULL);
  lock_init (&spt_lock);
}


static struct spt_entry *
create_spt_entry ()
{
  struct spt_entry *spt_entry = (struct spt_entry *) malloc (sizeof (struct spt_entry));
  spt_entry->frame = NULL;
  spt_entry->user_page = NULL;
  return spt_entry;
}

void
insert_spt_entry (void *user_page)
{
  struct spt_entry *spt_entry = create_spt_entry ();
  spt_entry->user_page = user_page;
  lock_acquire (&spt_lock);
  hash_insert (&spt, &spt_entry->elem);
  lock_release (&spt_lock);
}


void
free_spt_entry (struct hash_elem *e, void *aux)
{
  struct spt_entry *spt_entry = hash_entry (e, struct spt_entry, elem);
  if (spt_entry->frame != NULL)
    palloc_free_page (spt_entry->frame);
  free (spt_entry);
}


void destroy_spt (){
  lock_acquire (&spt_lock);
  hash_destroy (&spt, free_spt_entry);
  lock_release (&spt_lock);
}