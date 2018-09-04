/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
*/

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/timer.h"

/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
     decrement it.

   - up or "V": increment the value (and wake up one waiting
     thread, if any). */
void
sema_init (struct semaphore *sema, unsigned value) 
{
  ASSERT (sema != NULL);

  sema->value = value;
  list_init (&sema->waiters);
}

// static bool before_thread(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED)
// {
//   return list_entry(a, struct thread, elem)->priority > list_entry(b, struct thread, elem)->priority; 
// }
/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. */
void
sema_down (struct semaphore *sema) 
{
  enum intr_level old_level;

  ASSERT (sema != NULL);
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  
  /* Save seeking sema inside sema_down() to catch sema without lock*/
  struct thread *t = thread_current();
  t->sema_seeking = sema;
  
  while (sema->value == 0) 
  {
    /* Maintain descending order by priority for preemptive unblocking later in sema_up*/
    //list_push_back (&sema->waiters, &thread_current ()->elem);
    list_insert_ordered(&sema->waiters, &thread_current ()->elem, before_thread, NULL);
    thread_block ();
  }

  sema->value--;
  t->sema_seeking = NULL; /* Sema has been successfully downed */
  intr_set_level (old_level);
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool
sema_try_down (struct semaphore *sema) 
{
  enum intr_level old_level;
  bool success;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  if (sema->value > 0) 
    {
      sema->value--;
      success = true; 
    }
  else
    success = false;
  intr_set_level (old_level);

  return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
void
sema_up (struct semaphore *sema) 
{
  enum intr_level old_level;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  
  struct thread *th = NULL;
  if (!list_empty (&sema->waiters)) {
    /* Unblock the highest priority waiter first */
    th = list_entry (list_front (&sema->waiters), struct thread, elem);
    list_pop_front(&sema->waiters);
    thread_unblock (th);
  }
  sema->value++;
  intr_set_level (old_level);
  
  /* th will inserted to ready_list, so check for possible yield here only */
  if (th != NULL && th->priority > thread_current()->priority) thread_yield();
}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void) 
{
  struct semaphore sema[2];
  int i;

  printf ("Testing semaphores...");
  sema_init (&sema[0], 0);
  sema_init (&sema[1], 0);
  thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
  for (i = 0; i < 10; i++) 
    {
      sema_up (&sema[0]);
      sema_down (&sema[1]);
    }
  printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper (void *sema_) 
{
  struct semaphore *sema = sema_;
  int i;

  for (i = 0; i < 10; i++) 
    {
      sema_down (&sema[0]);
      sema_up (&sema[1]);
    }
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void
lock_init (struct lock *lock)
{
  ASSERT (lock != NULL);

  lock->holder = NULL;
  lock->priority = PRI_MIN; /* Since waiter's list is NULL */
  sema_init (&lock->semaphore, 1);
}

/* Compare two locks by their priority */
bool
before_lock (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED)
{
  struct lock *la = list_entry(a, struct lock, elem),
              *lb = list_entry(b, struct lock, elem);
  return la->priority > lb->priority;
}

/* Rescursively donates priority along lock holders, also sorts affected lists */
void priority_donate(struct lock* l, int extended_priority) {
  /* Base cases */
  if (l == NULL) return;
  if (l->priority >= extended_priority) return;
  if (l->holder == NULL) return;
  
  l->priority = extended_priority; /* Set lock's pririty */
  /* Donate priority to current lock holder's, if required */
  struct thread *t = l->holder;   /* t is l's holder */
  if (t->priority >= extended_priority) return;
  t->priority = extended_priority;
  /* Resore order in the waiter list of seeking semaphore (with/without lock) */
  if (t->sema_seeking != NULL) list_sort(&t->sema_seeking->waiters, before_thread, NULL);
  if (t->seeking != NULL) list_sort(&t->seeking->semaphore.waiters, before_thread, NULL);
  priority_donate(t->seeking, extended_priority); /* Recurse */
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
lock_acquire (struct lock *lock)
{
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (!lock_held_by_current_thread (lock));

  thread_current()->seeking = lock; /* Set the currently seeking lock */
  priority_donate(lock, thread_current()->priority);
  
  sema_down (&lock->semaphore); /* Comes below this line after a successful down. */
  
  /* Successfully acquired, insert to acquired_locks, set lock holder, also no longer seeking */
  list_insert_ordered(&thread_current()->acquired_locks, &lock->elem, before_lock, NULL);
  lock->holder = thread_current (); 
  thread_current()->seeking = NULL;
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool
lock_try_acquire (struct lock *lock)
{
  bool success;

  ASSERT (lock != NULL);
  ASSERT (!lock_held_by_current_thread (lock));

  // thread_current()->seeking = lock;
  success = sema_try_down (&lock->semaphore);
  if (success) {
    // list_push_back(&thread_current()->acquired_locks, &lock->elem);
    lock->holder = thread_current (); /* This line was present already */
    // thread_current()->seeking = NULL;
  }
  return success;
}

/* Releases LOCK, which must be owned by the current thread.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void
lock_release (struct lock *lock) 
{
  ASSERT (lock != NULL);
  ASSERT (lock_held_by_current_thread (lock));

  /* After lock_release has been called, lock release is imminent */
  list_remove(&lock->elem); /* Remove from acquired_locks */
  /* Find and set the new priority of the holder */
  struct thread *t = thread_current();
  thread_current()->priority = thread_current()->orig_priority; /* Default is priority before any donation */
  if (!list_empty(&t->acquired_locks)) { /* Overwrite if a greater donor is present */
    struct lock *l = list_entry (list_front( &t->acquired_locks), struct lock, elem);
    if (t->priority < l->priority) t->priority = l->priority;
  }

  sema_up (&lock->semaphore);
  /* Update attrubutes of released lock */
  lock->holder = NULL;
  lock->priority = PRI_MIN;
  struct semaphore *sema = &lock->semaphore;
  if (!list_empty(&sema->waiters)) { /* By def, lock priority is the highest priority waiter */
    struct thread *th = list_entry (list_front( &(sema->waiters)), struct thread, elem);
    lock->priority = th->priority;
  }
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock) 
{
  ASSERT (lock != NULL);

  return lock->holder == thread_current ();
}

/* One semaphore in a list. */
struct semaphore_elem
  { 
    struct list_elem elem;              /* List element. */
    struct semaphore semaphore;         /* This semaphore. */
  };

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void
cond_init (struct condition *cond)
{
  ASSERT (cond != NULL);

  list_init (&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
/* Compare two threads by priority encapsulated inside the semaphore of a condvar*/
bool
before_condvar(const struct list_elem *a,const struct list_elem *b,void *aux UNUSED)
{
  struct semaphore_elem *s1 = list_entry (a, struct semaphore_elem, elem);
  struct semaphore_elem *s2 = list_entry (b, struct semaphore_elem, elem);
  struct thread *l1 =list_entry (list_front (&((s1->semaphore).waiters)), struct thread, elem);  
  struct thread *l2 = list_entry (list_front (&((s2->semaphore).waiters)), struct thread, elem);
  return (l1->priority < l2->priority); 
}

void
cond_wait (struct condition *cond, struct lock *lock) 
{
  struct semaphore_elem waiter;

  ASSERT (cond != NULL);
  ASSERT (lock != NULL); 
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));
  
  sema_init (&waiter.semaphore, 0);
  /* Can just insert without order since list_max will be used while unblocking*/
  list_push_back (&cond->waiters, &waiter.elem);
  lock_release (lock);
  sema_down (&waiter.semaphore);
  lock_acquire (lock);
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));

  if (!list_empty (&cond->waiters)) {
    /* Extract maximum priority waiter using before_convar comparator */
    struct list_elem *l = list_max (&(cond->waiters), before_condvar, NULL);
    list_remove(l);
    struct semaphore_elem *s = list_entry (l, struct semaphore_elem, elem);
    sema_up(&s->semaphore);
  }
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_broadcast (struct condition *cond, struct lock *lock) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);

  while (!list_empty (&cond->waiters))
    cond_signal (cond, lock);
}
