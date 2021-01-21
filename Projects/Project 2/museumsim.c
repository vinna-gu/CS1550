/* museumsim.c Project 2 backup 1 */
#include <sys/mman.h>
#include <linux/unistd.h>
#include <stdio.h>
#include <sys/resource.h>

// Global variables
#define MAX_VISTORS_MUS 20;   // total num of visitors INSIDE museum
#define MAX_VISITORS_PER_GUIDE 10; // total num of visitors PER tour guide
#define MAX_TOUR_GUIDES 2; // total num of tour guides IN museum
struct timeval get_time;

// what needs to be shared among the processes?
typedef struct share_data {
  int visitors;             // num of visitors indicated by -m
  int tourGuides;           // num of tourGuides indicated by -k
  int availTickets;         // num of available tickets based off num of guides

  // who is inside the museum?
  int visitorsInMuseum;     // num of visitors currently in the museum
  int tourGuidesInMuseum;   // num of tour guides currently in the museum

  // who is waiting?
  int visitorsWaiting;      // num of vistors waiting to tour
  int tourGuidesWaiting;    // num of tour guides waiting to do a tour
} share_data;

typedef struct share_sem {
  // must also share the four waiting rooms between the processes as well
  long visitorRoom;         // visitor waiting room
  long tourGuideRoom;       // tour guide waiting room
  long museumRoom;          // museum waiting room
  long ticketRoom;          // ticket office room
  long lock;                // a lock to protect the methods & let them execute completely
} share_sem;

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
void visitorArrives(share_data* sdata_ptr, share_sem* semdata_ptr) {
  fprintf(stderr, "NUM TICKETS BEFORE = %d\n", sdata_ptr->availTickets);
  // first, make visitor wait in the waiting rooms
  sdata_ptr->visitorsWaiting++;
  // take a ticket
  sdata_ptr->availTickets--;

  fprintf(stderr, "NUM TICKETS AFTER= %d\n", sdata_ptr->availTickets);
}
void tourMuseum() {

}
void visitorLeaves() {

}
void tourGuideArrives() {

}
void openMuseum() {

}
void tourGuideLeaves() {

}

void guideProcess(void) {
  // arrive at museum, wait if theres no visitors, give tour, wait til all vistiors from both groups (or 1 group if its the last group), and leave it
  tourGuideArrives();
  openMuseum();
  tourMuseum();
  tourGuideLeaves();
  return;
}

void visitorProcess(share_data* sdata_ptr, share_sem* semdata_ptr) {
  // arrive at the museum, wait for a tourguide, tour it, leave it
  visitorArrives(sdata_ptr, semdata_ptr);
  tourMuseum();
  visitorLeaves();
  return;
}

// HELPER GUIDE PROCESSES
int guideHelper(int numGuides, share_data* sdata_ptr) {
  fprintf(stderr, "inside guideHelper + numGuides = %d\n", numGuides);
  // loop through the number of visitors
  int i = 0;
  while(i < numGuides) {
    int pid = fork();
    // if it is a child
    if(pid == 0) {
      fprintf(stderr, "%d: entering guideProcess here\n", i);
      guideProcess(); // do the guide stuff
      return 0;   // exit and do not fork to create another child
    }
    i += 1;
  }// end while loop
  return;
}

// HELPER VISITOR PROCESSES
int visitorHelper(int numVisitors, share_data* sdata_ptr, share_sem* semdata_ptr) {
  fprintf(stderr, "inside visitorHelper + numVisitors = %d\n", numVisitors); /* DELETE */

  // loop through the number of visitors
  int i = 0;
  while (i < numVisitors) {
    int pid = fork();
    // if it is a child
    if(pid == 0) {
      fprintf(stderr, "%d: entering visitorProcess here\n", i);
      visitorProcess(sdata_ptr, semdata_ptr); // do the visitor tour stuff
      return 0;   // exit and do not fork to create another child
    }
    i += 1;
  }// end while loop
  return; // leave
}

int main(int argc, char *argv[])
{
  // initializing values of the argv[] inputs
  int m = 0;    // the total number of visitors
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
  // initializing the values in the struct to 0, and creating the waiting rooms.
  sdata_ptr->visitors = 0;
  sdata_ptr->tourGuides = 0;
  sdata_ptr->visitorsInMuseum = 0;
  sdata_ptr->tourGuidesInMuseum = 0;
  sdata_ptr->availTickets = 0;
  sdata_ptr->visitorsWaiting = 0;
  sdata_ptr->tourGuidesWaiting = 0;

  // semaphore struct to be initialized AND shared among processes
  share_sem* semdata_ptr = mmap(NULL, sizeof(share_sem), PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, 0, 0);
  long lockSem_id = create(0, "Lock", "Lock");
  semdata_ptr->lock = lockSem_id;
  fprintf(stderr, "LOCK VALUE = %ld\n", semdata_ptr->lock);

  long tourGuideSem_id = create(0, "TourGuideRoom", "Tour Guide Waiting Room");
  semdata_ptr->tourGuideRoom = tourGuideSem_id;

  long visitorsSem_id = create(0, "VisitorRoom", "Visitor Waiting Room");
  semdata_ptr->visitorRoom = visitorsSem_id;
  fprintf(stderr, "VisitorROOM = %ld\n", semdata_ptr->visitorRoom);

  long museumSem_id = create(0, "MuseumRoom", "Museum Waiting Room");
  semdata_ptr->museumRoom = museumSem_id;

  long ticketSem_id = create(0, "TicketOffice", "Ticket Office Waiting Room");
  semdata_ptr->ticketRoom = ticketSem_id;

  //loop to make sure that all inputs m, k, pv, etc. have a value beside it
  int i = 0;
  while(i < argc) {
    if(strcmp(argv[i], "-m") == 0) {
      // if we don't find a valid number beside -m, we have to quit program
      if(atoi(argv[i+1]) == 0) {
        return 0;
      }
      m = atoi(argv[i+1]);
      sdata_ptr->visitors = m; // storing into shared_data so processes can see how many visitors
    }
    else if (strcmp(argv[i], "-k") == 0) {
      if(atoi(argv[i+1]) == 0) {
        return 0;
      }
      k = atoi(argv[i+1]);
      sdata_ptr->tourGuides = k; // storing into shared_data
      int ticketTotal = k * MAX_VISITORS_PER_GUIDE;
      sdata_ptr->availTickets = ticketTotal;
    }
    else if (strcmp(argv[i], "-pv") == 0) {
      if(atoi(argv[i+1]) == 0) {
        return 0;
      }
      pv = atoi(argv[i+1]);
    }
    else if (strcmp(argv[i], "-dv") == 0) {
      if(atoi(argv[i+1]) == 0) {
        return 0;
      }
      dv = atoi(argv[i+1]);
    }
    else if (strcmp(argv[i], "-sv") == 0) {
      if(atoi(argv[i+1]) == 0) {
        return 0;
      }
      sv = atoi(argv[i+1]);
    }
    else if (strcmp(argv[i], "-pg") == 0) {
      if(atoi(argv[i+1]) == 0) {
        return 0;
      }
      pg = atoi(argv[i+1]);
    }
    else if (strcmp(argv[i], "-dg") == 0) {
      if(atoi(argv[i+1]) == 0) {
        return 0;
      }
      dg = atoi(argv[i+1]);
    }
    else if (strcmp(argv[i], "-sg") == 0) {
      if(atoi(argv[i+1]) == 0) {
        return 0;
      }
      sg = atoi(argv[i+1]);
    }
    i += 1; // check the next args
  } // end while loop for args[]

  // forking from the main process
  int pid = fork();

  // if it is a child, we are gonna create a "visitor generator" as the child of this parent process
  if(pid == 0) {
    visitorHelper(sdata_ptr->visitors, sdata_ptr, semdata_ptr);   // visitor generator process
  }
  // if it is a parent, we can create individual tourguides
  else if(pid > 0) {
    guideHelper(sdata_ptr->tourGuides, sdata_ptr);
  }

  return 0;   // otherwise exit program
}
