#ifndef VM_PAGE
#define VM_PAGE

#include <hash.h>


struct spt_entry
  {
    void *user_page;
    void *frame;
    struct hash_elem elem;
  };

#endif