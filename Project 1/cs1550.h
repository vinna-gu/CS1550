#ifndef _LINUX_CS1550_H
#define _LINUX_CS1550_H 1
#include <linux/smp_lock.h>

typedef struct cs1550_sem // semaphore data type
{
   // CONTENTS OF MY SEMAPHORE
   int value;
   long sem_id;                       // long integer identifier (create a global to increment it or random gen)
   spinlock_t lock;
   char key[32];
   char name[32];                     // name of the semaphore
   // FIFO linked list to hold my processes
   struct cs1550_fifoProc* procHead;  // points to the head of the linkedlist of processes
   struct cs1550_fifoProc* procTail; // points to the tail (what we remove in FIFO since it's the first thing ) 

   // point to the next semaphore node
   struct cs1550_sem* nextSem;

} cs1550_sem;
//Add to this file any other struct definitions that you may need

// FIFO Queue: curr process that points to next process, & curproc
typedef struct cs1550_fifoProc{
  struct task_struct* currProcData;   // similar to struct proc
  struct cs1550_fifoProc* procNext;
} cs1550_fifoProc;


#endif
