#include <sys/mman.h>
#include <linux/unistd.h>
#include <stdio.h>
#include <sys/resource.h>
#include <stdlib.h>     /* srand, rand */

// Global variables
#define MAX_VISITORS_MUS 20   // total num of visitors INSIDE museum
#define MAX_VISITORS_PER_GUIDE 10 // total num of visitors PER tour guide
#define MAX_TOUR_GUIDES 2 // total num of tour guides IN museum
struct timeval startTime;
// what needs to be shared among the processes?
typedef struct share_data {
  // initialized variables from the main function
  int visitors;             // num of visitors indicated by -m
  int tourGuides;           // num of tourGuides indicated by -k
  int availTickets;         // num of available tickets based off num of guides that decide to volunteer that day
  int isMusOpen;            // is museum open for customers yet?

  // who is waiting to tour the museum?
  int visitorsWaiting;      // num of vistors waiting to tour
  int tourGuidesWaiting;    // num of tour guides waiting to do a tour

  // who is inside the museum?
  int visitorsInMuseum;     // num of visitors currently in the museum
  int tourGuidesInMuseum;   // num of tour guides currently in the museum

  // how many people did the tourguide serve?
  int servedVisitors;
  int noTicketVisitors;     // when visitor is unable to obtain a ticket, we want them to leave

  // must also share the four waiting rooms between the processes as well
  long visitorRoom;         // visitor waiting room
  long tourGuideRoom;       // tour guide waiting room
  long museumRoom;          // museum waiting room
  long lock;                // a lock to protect the methods & also to protect the above variables
} share_data;

long create(int value, char name[32], char key[32]) {
  return syscall(__NR_cs1550_create, value, name, key);
}

long open(char name[32], char key[32]) {
  return syscall(__NR_cs1550_open, name, key);
}

long down(long sem_id) {
  return syscall(__NR_cs1550_down, sem_id);
}

long up(long sem_id) {
  return syscall(__NR_cs1550_up, sem_id);
}

long close(long sem_id) {
  return syscall(__NR_cs1550_close, sem_id);
}

// The 6 functions needed to be called by visitor & guide processes
void visitorArrives(int visitorID, share_data* sdata_ptr) {
  down(sdata_ptr->lock);  // FIRST, lock to protect shared variables
  struct timeval endTime;
  gettimeofday(&endTime, NULL);

  // if there are no tickets
  if(sdata_ptr->availTickets == 0) {
    int time = (endTime.tv_sec - startTime.tv_sec);
    fprintf(stderr, "Visitor %d arrives at time %d.\n", visitorID, time);
    sdata_ptr->noTicketVisitors++;
  }

  // if there's a ticket available
  if(sdata_ptr->availTickets > 0) {
     sdata_ptr->availTickets--;      // take a ticket
     sdata_ptr->visitorsWaiting++;   // visitor should first wait in the waiting room
     int time = (endTime.tv_sec - startTime.tv_sec);
     fprintf(stderr, "Visitor %d arrives at time %d.\n", visitorID, time);

     // if the museum is closed OR the capacity of the museum is full (TG * 10 ...since they can only serve 10 per TG)
     if( (sdata_ptr->isMusOpen == 0) || (sdata_ptr->visitorsInMuseum == (MAX_VISITORS_PER_GUIDE * sdata_ptr->tourGuidesInMuseum)) ) {
       up(sdata_ptr->lock);           // unlock here to let us down visitorRoom
       down(sdata_ptr->visitorRoom);  // visitor process should wait in the waiting room until it gets upped
       down(sdata_ptr->lock);         // lock again to protect the variables below
     }
     // othrwise, the museum is open or it isn't full so alert a tourguide that you are here
     up(sdata_ptr->tourGuideRoom);    // wake up guide to alert them that a visitor is here
     sdata_ptr->visitorsWaiting--;    // visitor now leaves the waiting room
     sdata_ptr->visitorsInMuseum++;   // visitor has permission to go in to the museum for a tour
   }
  up(sdata_ptr->lock);  // release the lock
} // END visitorArrives()
void tourMuseum(int visitorID, share_data* sdata_ptr) {
  down(sdata_ptr->lock);  // just to protect
  struct timeval endTime;
  gettimeofday(&endTime, NULL);

  int time = (endTime.tv_sec - startTime.tv_sec);

  // if there are noTicketVisitors...
  if(sdata_ptr->noTicketVisitors > 0) {
    up(sdata_ptr->lock);
  }
  else {
    fprintf(stderr,"Visitor %d tours the museum at time %d.\n", visitorID,time);
    up(sdata_ptr->lock);
    sleep(2); // allow the visitor to spend a few seconds to tour the museum
  }

}
void visitorLeaves(int visitorID, share_data* sdata_ptr) {
  down(sdata_ptr->lock);  // lock
  struct timeval endTime;
  gettimeofday(&endTime, NULL);
  int time = (endTime.tv_sec - startTime.tv_sec);
  fprintf(stderr,"Visitor %d leaves the museum at time %d.\n", visitorID, time);
  // if visitor was in the museum, let them leave
  if(sdata_ptr->visitorsInMuseum > 0) {
    sdata_ptr->visitorsInMuseum--; // leave the museum
  }

  // if we removed the last visitor in the museum, no more waiting visitors, no more visitors with notickets && there's 1 tourguides let guide(s) leave
  if(sdata_ptr->visitorsInMuseum == 0 && sdata_ptr->visitorsWaiting > 0 && sdata_ptr->noTicketVisitors == 0 && sdata_ptr->tourGuidesInMuseum == 1) {
    int i = 0;    // counter to wake up a number of guides
    while(sdata_ptr->tourGuidesInMuseum > 0) {
      up(sdata_ptr->museumRoom);              // release the tour guide
      sdata_ptr->tourGuidesInMuseum--;        // decrement the number of tourGuidesInMuseum
      if(sdata_ptr->tourGuidesWaiting > 0 && i < MAX_TOUR_GUIDES) {  // if there's a tour guide waiting to do a tour, wake them up
      }
    }// end while loop
  }// end if (sdata_ptr->visitorsInMuseum == 0)
  // if we removed the last visitor in the museum, no more waiting visitors, no more visitors with notickets && there's 2 tourguides let guide(s) leave
  else if(sdata_ptr->visitorsInMuseum == 0 && sdata_ptr->visitorsWaiting == 0 && sdata_ptr->noTicketVisitors == 0 && sdata_ptr->tourGuidesInMuseum <= MAX_TOUR_GUIDES) {
    int i = 0;    // counter to wake up a number of guides
    while(sdata_ptr->tourGuidesInMuseum > 0) {
      up(sdata_ptr->museumRoom);              // release the tour guide
      sdata_ptr->tourGuidesInMuseum--;        // decrement the number of tourGuidesInMuseum
      if(sdata_ptr->tourGuidesWaiting > 0 && i < MAX_TOUR_GUIDES) {  // if there's a tour guide waiting to do a tour, wake them up
      }
    }// end while loop
  }// end if (sdata_ptr->visitorsInMuseum == 0)


  // if there's a noTicketVisitors, we need to decrement the variable
  if(sdata_ptr->noTicketVisitors > 0) {
    sdata_ptr->noTicketVisitors--;
    up(sdata_ptr->lock);
    down(sdata_ptr->lock);
  }
  up(sdata_ptr->lock);
}
void tourGuideArrives(int tourID, share_data* sdata_ptr) {
  down(sdata_ptr->lock);            // lock to protect below variables
  struct timeval endTime;
  gettimeofday(&endTime, NULL);
  int time = (endTime.tv_sec - startTime.tv_sec);
  sdata_ptr->tourGuidesWaiting++;   // first, tourguide should wait and observe: who's here, is the museum full, is the museum open?
  fprintf(stderr, "Tour guide %d arrives at time %d.\n", tourID, time );
  // if (no visitors waiting but thre are visitors) OR (museum isn't open & there aren't visitors) OR (museum is open but w/ 2 tour guides touring)
  if( (sdata_ptr->visitorsWaiting <= 0 && sdata_ptr->visitors > 0) || (sdata_ptr->isMusOpen == 0 && sdata_ptr->visitorsWaiting <= 0) || (sdata_ptr->isMusOpen == 1 && sdata_ptr->tourGuidesInMuseum == MAX_TOUR_GUIDES) ){
     up(sdata_ptr->lock);             // protect
     down(sdata_ptr->tourGuideRoom);  // tourguide must wait in the waitingroom until they are alerted by the visitor or another tour guide that they may go
     down(sdata_ptr->lock);           // re-lock for below variables
   }
   // otherwise, museum isn't open && there ARE visitors || museum is open && THERE ARE visitors || museum is open but with < 2 guides
   sdata_ptr->tourGuidesWaiting--;    // stop waiting and start to give tours to the visitors
   sdata_ptr->tourGuidesInMuseum++;   // start to do tours

   //if there aren't any visitors to begin with...
   if(sdata_ptr->visitors == 0) {
     sdata_ptr->tourGuidesInMuseum--; // don't increment the number of tourguides
   }
   up(sdata_ptr->lock);               // release the lock
} // END tourGuideArrives()

void openMuseum(int tourID, share_data* sdata_ptr) {
  down(sdata_ptr->lock);      // lock
  struct timeval endTime;
  gettimeofday(&endTime, NULL);
  int time = (endTime.tv_sec - startTime.tv_sec);
  if(sdata_ptr->visitors > 0 && sdata_ptr->visitorsWaiting) {
    fprintf(stderr, "Tour guide %d opens the museum for tours at time %d.\n", tourID, time);
    // if the museum isn't open
    if(sdata_ptr->isMusOpen == 0) {
      sdata_ptr->isMusOpen = 1;   // set museum to open
    }
    int i = 0;
    // the museum is now open so alert the waiting visitors that the museum is open
    while(i < sdata_ptr->visitorsWaiting) {
      // as long as we hav served less 10 visitors
      if(sdata_ptr->servedVisitors < 10) {
        up(sdata_ptr->visitorRoom);   // wake the visitor
        sdata_ptr->servedVisitors++;  // increment the number of visitors we serve for this tourguide
      }
      i += 1;                       // increment to see if there's another visitor waiting
    }
    if(sdata_ptr->servedVisitors == 10) {
      sdata_ptr->servedVisitors = 0;
    }
  }


  up(sdata_ptr->lock);  //unlock
}
void tourGuideLeaves(int tourID, share_data* sdata_ptr) {
  /* Tour Guide Leaves RULES
      -- Tour guides that are inside the museum cannot leave until all visitors inside the museum leave.
      -- A guide cannot leave until they serve ten visitors (the only exception is when there are no
        remaining tickets, in which case the guide can leave without serving ten visitors).
      -- Guides that happen to be inside the museum must wait for each other and leave together.
      -- Meeting all previous conditions, guides must leave as soon as they can to get back to work on projects
  */
  down(sdata_ptr->lock);  // lock
  struct timeval endTime;

  // FIRST if there are noTicketVisitors and there's visitors still inside museum or visitors STILL inside the museum OR there's one other tourguide inside
  if(sdata_ptr->tourGuidesInMuseum == 1 && ((sdata_ptr->noTicketVisitors > 0 && sdata_ptr->visitorsInMuseum > 0) || sdata_ptr->visitorsInMuseum > 0 || (sdata_ptr->servedVisitors != 10 && sdata_ptr->visitorsWaiting > 0))) {
    up(sdata_ptr->lock);
    down(sdata_ptr->museumRoom);    // wait in museumRoom until no visitors, or other tourguide is done
    down(sdata_ptr->lock);
  }
  // FIRST 2 tourguides in museum if there are noTicketVisitors and there's visitors still inside museum or visitors STILL inside the museum OR there's one other tourguide inside
  else if(sdata_ptr->tourGuidesInMuseum == MAX_TOUR_GUIDES && ((sdata_ptr->noTicketVisitors > 0 && sdata_ptr->visitorsInMuseum > 0) || sdata_ptr->visitorsInMuseum > 0 || (sdata_ptr->servedVisitors != 10 && sdata_ptr->visitorsWaiting > 0))) {
    up(sdata_ptr->lock);
    down(sdata_ptr->museumRoom);    // wait in museumRoom until no visitors, or other tourguide is done
    down(sdata_ptr->lock);
  }
  gettimeofday(&endTime, NULL);
  int time = (endTime.tv_sec - startTime.tv_sec);

  fprintf(stderr,"Tour guide %d leaves the museum at time %d\n", tourID, time);
  up(sdata_ptr->lock);
}

void guideProcess(int procID, share_data* sdata_ptr) {
  tourGuideArrives(procID, sdata_ptr);
  openMuseum(procID, sdata_ptr);
  tourGuideLeaves(procID, sdata_ptr);
  exit(0);  // need to terminate the guide process now
}

void visitorProcess(int procID, share_data* sdata_ptr) {
  // visitor first arrives
  visitorArrives(procID, sdata_ptr);
  tourMuseum(procID, sdata_ptr);
  visitorLeaves(procID, sdata_ptr);
  exit(0);  // need to terminate this process now
}

// HELPER GUIDE PROCESSES
void guideHelper(int numGuides, share_data* sdata_ptr, int dg, int pg) {
  int i = 0;  // first while loop to create guide processes
  int j = 0;  // second while loop to wait for all the guide processes we created
  int delay;
  // loop through the number of guides
  while(i < numGuides) {
    int pid = fork();
    // if it is a child
    if(pid == 0) {
      guideProcess(i, sdata_ptr); // do the guide stuff
      exit(0);   // exit and do not fork to create another child
    }
    else if (pid > 0) {
      delay = rand() % 100 + 1; // range + min
      if(pg < delay) {
        sleep(dg);
      }
    } // end else if
    i += 1;
  }// end while loop

  // wait for the guide processes
  for(j; j < numGuides; j++) {
    wait(NULL);
  }
  return;
}

// HELPER VISITOR PROCESSES
void visitorHelper(int numVisitors, share_data* sdata_ptr, int dv, int pv) {
  int i = 0;
  int j = 0;
  int delay;

  // loop through the number of visitors
  while(i < numVisitors) {
    int pid = fork();
    // if it is a child
    if(pid == 0) {
      visitorProcess(i, sdata_ptr); // do the visitor tour stuff
      exit(0);   // break out of this loop to prevent from going back to loop after finishing tour
    }
    else if (pid > 0) {
      delay = rand() % 100 + 1; // range + min
      if(pv < delay) {
        sleep(dv);
      }
    } // end else if
    i += 1;
  }// end while loop

  // After generator finishes creation, wait for all the numVisitor processes to terminate
  for(j; j < numVisitors; j++) {
    wait(NULL);
  }
  return; // leave
}

int main(int argc, char *argv[]) {
  // initializing values of the argv[] inputs
  int m = 0;    // total num of visitors
  int k = 0;    // total num of guides
  int pv = 0;   // prob. of a visitor immediately following another visitor
  int dv = 0;   // delay in seconds when a visitor doesn't immediately follow another visitor
  int sv = 0;   // random seeed for visitor arrival process
  int pg = 0;   // probabiltiy of tour guide immediately following another tour guide
  int dg = 0;   // delay in seconds when tour guide doesn't
  int sg = 0;   // random seed for tour guide arrival process

  // user must input at least 17 arguments or exit the program;
  if(argc < 17 || argc > 17) {
    fprintf(stderr, "Please enter in this format: ./museumsim -m [num] -pv [num] -dv [num] -sv [num] -k [num] -pg [num] -dg [num] -sv [num]\n");
    return 0; // exit out of program immediately
  }
  share_data* sdata_ptr = mmap(NULL, sizeof(share_data), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);   // this returns the address of the shared memory
  // get time portion of the project
  gettimeofday(&startTime, NULL);  // 2nd parameter is the timezone, also this marks the time the program starts
  // initializing the values in the struct to 0, and creating the waiting rooms.
  sdata_ptr->visitors = 0;
  sdata_ptr->tourGuides = 0;
  sdata_ptr->isMusOpen = 0;     // museum isn't open yet
  sdata_ptr->visitorsInMuseum = 0;
  sdata_ptr->tourGuidesInMuseum = 0;
  sdata_ptr->availTickets = 0;
  sdata_ptr->visitorsWaiting = 0;
  sdata_ptr->tourGuidesWaiting = 0;
  // initializing the semaphores
  sdata_ptr->lock = create(1, "Lock", "Lock for mutual exclusion");                       // 1 allows us to protect variables
  sdata_ptr->tourGuideRoom = create(0, "TourGuideRoom", "Tour Guide Waiting Room");       // 0 for TG's to wait in the tourGuideRoom, when negative it means processes are waiting
  sdata_ptr->visitorRoom = create(0, "Visitor Room", "Visitor Waiting Room");             // 0 for visitors to wait
  sdata_ptr->museumRoom = create(0, "Museum Room", "Museum Waiting Room");                // 0 for TG's to wait for each other before leaving

  //loop to make sure that all inputs m, k, pv, etc. have a value beside it
  int i = 0;
  while(i < argc) {
    if(strcmp(argv[i], "-m") == 0) {
      // if we don't find a valid number beside -m, we have to quit program
      if(atoi(argv[i+1]) < 0) {
        return 0;
      }
      m = atoi(argv[i+1]);
      sdata_ptr->visitors = m; // storing m into sdata_ptr->visitors to indicate how many customers we'll be expecting
    }
    else if (strcmp(argv[i], "-k") == 0) {
      if(atoi(argv[i+1]) < 0) {
        return 0;
      }
      k = atoi(argv[i+1]);
      sdata_ptr->tourGuides = k;                      // storing k into sdata_ptr->tourGuides to indicate how many tourGuides will be doing tours that day
      int ticketTotal = k * MAX_VISITORS_PER_GUIDE;   // determines number of tickets that can be possibly sold to visitors
      sdata_ptr->availTickets = ticketTotal;
    }
    else if (strcmp(argv[i], "-pv") == 0) {
      if(atoi(argv[i+1]) < 0) {
        return 0;
      }
      pv = atoi(argv[i+1]);
    }
    else if (strcmp(argv[i], "-dv") == 0) {
      if(atoi(argv[i+1]) < 0) {
        return 0;
      }
      dv = atoi(argv[i+1]);
    }
    else if (strcmp(argv[i], "-sv") == 0) {
      if(atoi(argv[i+1]) < 0) {
        return 0;
      }
      sv = atoi(argv[i+1]);
    }
    else if (strcmp(argv[i], "-pg") == 0) {
      if(atoi(argv[i+1]) < 0) {
        return 0;
      }
      pg = atoi(argv[i+1]);
    }
    else if (strcmp(argv[i], "-dg") == 0) {
      if(atoi(argv[i+1]) < 0) {
        return 0;
      }
      dg = atoi(argv[i+1]);
    }
    else if (strcmp(argv[i], "-sg") == 0) {
      if(atoi(argv[i+1]) < 0) {
        return 0;
      }
      sg = atoi(argv[i+1]);
    }
    i += 1; // check the next args
  } // end while loop for args[]
  fprintf(stderr, "The museum is now empty.\n");

  // forking from the main process
  int pid = fork();
  // if it is a child, we are essentially using this child as a "visitor generator" (THIS IS A CHILD OF MAIN)
  if(pid == 0) {
    srand(sv);  // initialize here only once to set the seed used by the random number generator (rand) for visitors
    visitorHelper(sdata_ptr->visitors, sdata_ptr, dv, pv);   // creating visitor processes inside this method (these will be the grand children of main)
  }
  // Main is the parent of all the guides we'll b creating
  else if(pid > 0) {
    srand(sg);  // initialize here only once to set the seed used by the random number generator (rand) for guides
    guideHelper(sdata_ptr->tourGuides, sdata_ptr, dg, pg);  // creating guideProcesses through the parent (main process)

  }
  wait(NULL); // wait for the visitor generator to terminate
  close(sdata_ptr->lock);
  close(sdata_ptr->tourGuideRoom);
  close(sdata_ptr->visitorRoom);
  close(sdata_ptr->museumRoom);
  return 0;   // otherwise exit program
}
