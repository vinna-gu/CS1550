#include <linux/cs1550.h>
DEFINE_SPINLOCK(sem_list_lock);     // globally defined spinlock to protect the semList
int semID = 0;                      // globally defined int to ++ when new semaphors are made
cs1550_sem* semaphoreHead = NULL;   // head pointer

/* This syscall creates a new semaphore and stores the provided key to protect access to the semaphore. The integer value is used to initialize the semaphore's value. The function returns the identifier of the created semaphore, which can be used to down and up the semaphore. */
asmlinkage long sys_cs1550_create(int value, char name[32], char key[32]){
  cs1550_sem* newSemaphore;   // the newly allocated semaphore will be set to this
  cs1550_sem* traverseSem;

  spin_lock(&sem_list_lock);  // to protect the global semaphore list
  newSemaphore = (cs1550_sem*)kmalloc(sizeof(cs1550_sem), GFP_KERNEL);

  //initialize a spin lock for the new semaphore that has been created to protect the shared data below
  spin_lock_init(&newSemaphore->lock);

   //initialize newSemaphore's value, sem_id, key, name, and the pointers of the linkedlists
  newSemaphore->value = value;
  newSemaphore->sem_id = (semID + 1);
  strcpy(newSemaphore->name, name);
  strcpy(newSemaphore->key, key);

  // if the head of the semaphore is initialized to NULL, we need to reassign it, and set the newSemaphore = to it, and handle it's next
  if(semaphoreHead == NULL) {
    semaphoreHead = newSemaphore;
    newSemaphore->nextSem = NULL;
  }
  else if(semaphoreHead != NULL) {
    traverseSem = semaphoreHead;  // initialize the traversing semaphore

    // while the traversing semaphore isn't null, keep moving right until we see that it's next is NULL
    while(traverseSem->nextSem != NULL) {
      traverseSem = traverseSem->nextSem;
    }
    // once we see nextSem == NULL, set it's netSem = new semaphore, and set the new semaphore's netx == NULL;
    traverseSem->nextSem = newSemaphore;
    newSemaphore->nextSem = NULL;
    // because new semaphore won't have any processes just yet, initialize its processHead/tail to null
    newSemaphore->procHead = NULL;
    newSemaphore->procTail = NULL;
  }
  spin_unlock(&sem_list_lock);
  return newSemaphore->sem_id;
}
/* This syscall opens an already created semaphore by providing the semaphore name and the correct key. The function returns the identifier of the opened semaphore if the key matches the stored key or -1 otherwise. */
asmlinkage long sys_cs1550_open(char name[32], char key[32]){
  cs1550_sem* traverseSem = semaphoreHead;
  spin_lock(&sem_list_lock);
  // while the traverse sem isn't null, keep traversing and see see if there's a match
  while(traverseSem != NULL) {
    // if match, unlock the semaphore lock, and return 0 to confirm it is valid
    if(strcmp(traverseSem->name, name) == 0 && strcmp(traverseSem->key, key) == 0 ) {
      spin_unlock(&sem_list_lock);
      return 0;
    }
    traverseSem = traverseSem->nextSem;
  }

  // at this point, it is not a valid semaphore so just return -1
  spin_unlock(&sem_list_lock);
  return -1;
}
/* This syscall implements the down operation on an already opened semaphore using the semaphore identifier obtained from a previous call to sys_cs1550_create or sys_cs1550_open. The function returns 0 when successful or -1 otherwise (e.g., if the semaphore id is invalid or if the queue is full). Please check the lecture slides for the pseudo-code of the down operation. */
asmlinkage long sys_cs1550_down(long sem_id){ // Down = Wait
  cs1550_sem* traverseSem = semaphoreHead;
  cs1550_fifoProc* p;

  // lock global lock to traverse through the semaphore list, find the correct semaphore we want, and break out, else, it's invalid so return -1
  spin_lock(&sem_list_lock);
  while(traverseSem != NULL) {
    if(traverseSem->sem_id == sem_id) {
      traverseSem->sem_id = sem_id;
      break;
    }
    traverseSem = traverseSem->nextSem;
  }
  // if there isn't a match, return -1 immediately
  if(traverseSem->sem_id != sem_id) {
    spin_unlock(&sem_list_lock);
    return -1;
  }
  spin_unlock(&sem_list_lock);

  // PROCLIST: time to lock the semaphore's lock
  spin_lock(&traverseSem->lock);
  traverseSem->value = (traverseSem->value - 1);
  // if the value is < 0, we need to add the process into the process FIFO list, and then sleep
  if(traverseSem->value < 0) {
    p = (cs1550_fifoProc*)kmalloc(sizeof(cs1550_fifoProc), GFP_KERNEL); // allocate space for the newprocess

    // first, if we can't kmalloc, the queue is most likely full, unlock and return -1
    if(!p) {
      spin_unlock(&traverseSem->lock);
      return -1;
    }
    p->currProcData = current;
    // if the head of the list is empty, point it there
    if(traverseSem->procHead == NULL && traverseSem->procTail == NULL) {
      traverseSem->procHead = p;
      traverseSem->procTail = p;
      p->procNext = NULL;
    }
    // if the head of the list isn't empty, but the next is
    else if(traverseSem->procHead != NULL && traverseSem->procHead->procNext == NULL) {
      traverseSem->procHead->procNext = p;  // set the head's next = to new proc
      p->procNext = NULL;
      traverseSem->procTail = p; // point the tail to the new proc
    }
    // if the head & the next isn't... we need to jump to the tail and set it's next to the tail's next, and reassign tail to p
    else {
      traverseSem->procTail->procNext = p;
      traverseSem->procTail = p;
    }

    // need to unlock BEFORE sleeping or there's going to be an issue with proceseses being unable to acquire locks!
    spin_unlock(&traverseSem->lock);
    set_current_state(TASK_INTERRUPTIBLE);        // Mark the task as not ready (but can be awoken by signals)
    schedule();
    return 0;
  }
  else {
    spin_unlock(&traverseSem->lock);
    return 0;
  }

}
/* This syscall implements the up operation on an already opened semaphore using the semaphore identifier obtained from a previous call to sys_cs1550_create or sys_cs1550_open. The function returns 0 when successful or -1 otherwise (e.g., if the semaphore id is invalid). Please check the lecture slides for pseudo-code of the up operation. */
asmlinkage long sys_cs1550_up(long sem_id){ // Up = Signal
  cs1550_sem* traverseSem = semaphoreHead;
  cs1550_fifoProc* p = traverseSem->procHead;

  // SEMLIST: lock global lock to traverse through the semaphore list, find the correct semaphore we want, and break out, else, it's invalid so return -1
  spin_lock(&sem_list_lock);
  while(traverseSem != NULL) {
    if(traverseSem->sem_id == sem_id) {
      traverseSem->sem_id = sem_id;
      break;
    }
    traverseSem = traverseSem->nextSem;
  }
  // if there isn't a match, return -1 immediately
  if(traverseSem->sem_id != sem_id) {
    spin_unlock(&sem_list_lock);
    return -1;
  }
  spin_unlock(&sem_list_lock);

  // PROCLIST: time to lock the semaphore's lock
  spin_lock(&traverseSem->lock);
  traverseSem->value = (traverseSem->value + 1);

  // remove process from the FIFO list
  if(traverseSem->value <= 0 && traverseSem->procHead != NULL) {

    // if the head of the list isn't null, and it's next is, just remove the head
    if(traverseSem->procHead->procNext == NULL) {
      traverseSem->procHead = NULL;
      traverseSem->procTail = NULL;
    }
    // if the head of hte list isn't null, and the next also isn't, reassign the prochead
    else if(traverseSem->procHead->procNext != NULL) {
      traverseSem->procHead = traverseSem->procHead->procNext;
    }
    wake_up_process(p->currProcData);
    kfree(p);
    spin_unlock(&traverseSem->lock);
    return 0;
  }
  // otherwise, we need to get out because there's no procsses needed to be removed
  else {
    spin_unlock(&traverseSem->lock);
    return 0;
  }
}
/* This syscall removes an already created semaphore from the system-wide semaphore list using the semaphore identifier obtained from a previous call to sys_cs1550_create or sys_cs1550_open. The function returns 0 when successful or -1 otherwise (e.g., if the semaphore id is invalid or the semaphore's process queue is not empty). */
asmlinkage long sys_cs1550_close(long sem_id){
  cs1550_sem* traverseSem = semaphoreHead;
  cs1550_sem* prevSem = traverseSem;
  cs1550_sem* removeSem = traverseSem;

  // first, lck the semaphore list lock
  spin_lock(&sem_list_lock);

  // while there is something in the list
  while(traverseSem != NULL) {
    // if the semaphore right now does not match the id we want, keep traversing the list
    if(traverseSem->sem_id != sem_id) {
      prevSem = traverseSem;  // point prevSem = to traverseSem to keep it consistnely bhind traverseSem
      traverseSem = traverseSem->nextSem; // MOVE ONLY THE TRAVERSING SEM
    }
    // otherwise, traverseSem->sem_id == sem_id! but now we have to see where it is in the list
    else {
      // return -1 if proc queue isnt' empty
      // if we're at the head of the list and it doesn't even have anything next to it set it to NULL
      if(traverseSem == semaphoreHead && traverseSem->nextSem == NULL ) {
        removeSem = traverseSem;
        traverseSem = NULL;
        prevSem = NULL;
        semaphoreHead = NULL;
      }
      // if we're at head of list, and it's next isn't null, set semaphoreHead == next, prev == next,
      else if(traverseSem->nextSem != NULL && traverseSem == semaphoreHead ) {
        removeSem = traverseSem;
        prevSem = traverseSem->nextSem;
        semaphoreHead = traverseSem->nextSem;
      }
      // if we're at the middle of the list
      else if(traverseSem->nextSem != NULL && traverseSem != semaphoreHead) {
        removeSem = traverseSem;
        // set the prev's next = traverseSem's next, and traverse = traverseSem's next
        prevSem->nextSem = traverseSem->nextSem;
        traverseSem = traverseSem->nextSem;
      }
      // we're at the end of the list
      else if(traverseSem->nextSem == NULL) {
        removeSem = traverseSem;
        prevSem->nextSem = NULL;
      }
      kfree(removeSem);
      spin_unlock(&sem_list_lock);
      return 0;
    }
  }

  // at this point, the traversing did not turn up anything so just return -1
  spin_unlock(&sem_list_lock);
  return -1;
}
