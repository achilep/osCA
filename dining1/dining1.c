/*============================================================================

  The DINING PHILOSOPHERS program

   
  COMPILE: gcc -Wall -Werror -o philosopher philosopher.c -L. -lipc

  USAGE:   ./philosopher n m 
  
  where:   n= number of philosophers 
	   m=amount of rice on the plate (philosophers eat one unit of 
	   riceevery time they eat)
  

  If you press ctr-c the program will stop. This can take a couple of
  seconds. If you are interested in why, have a look in the code and
  try to figure it out.

  NOTE: the ouptut may seem to come in a strange order due to
  interleaving of processes and buffered output.


  HISTORY
  
  2003-02-11	Karl Marklund (karl.marklund@it.uu.se)
  
  1st version, implementing the deadlock free solution (2) bellow.

  (0) Allow one chair to be empty. You are NOT ALLOWED TO USE THIS
  METHOD in your solution.If you want to try this, define CHEATING
  below.
  
  (1) Allow a philosopher to pickup her chopsticks only if both
  chopsticks are available (to do this she must pick them up in a
  critical section). To try this you must create and use a new
  semaphore. Make sure to leave no ipc-objects! You may modify the
  clean_exit() function.

  (2) Use an asymmetric solution; that is an odd philosopher picks up
  first her left chopstick and then her right chopstick, wheras an
  even philosopher picks up first her right chopstick and then her
  left chopstick. To try this you may use the is_odd() function.

  --------------------------------------------------------------------------

  
  2003-02-17	Karl Marklund (karl.marklund@it.uu.se)

  Added duration(min,max) to make random sleep times...

  --------------------------------------------------------------------------
  

  2003-02-xx	  Student1Name (your.email@student.uu)
		  Student2Name (your.email@student.uu.se)
  
  Add a short descripion of your implementation here....


============================================================================*/

//#define CHEATING  // Unncoment if you want to try solution (0)


#include <stdio.h>
#include <stdlib.h>  
#include <unistd.h>        
#include <sys/wait.h>
#include <signal.h>

#include <sys/types.h>
#include <time.h>

#include "ipc.h"

#define SLOW 20	// Used to make one philosopher slower than the rest to
		// make starvation more likely.

#define EATING();	sleep(duration(3,7));
#define THINKING();	sleep(duration(2,10));
#define PICK_UP();	sleep(duration(1,3));
#define PUT_DOWN();	sleep(duration(1,3));
#define LEAVING();	sleep(duration(2,5));


#define	  DELFAULT_NUM_OF_PHILOSOPHERS  5
#define	  DELFAULT_RICE 25

int num_philosophers=DELFAULT_NUM_OF_PHILOSOPHERS; // Ugly global counter ;(


// chopstick macros 

#define   Left(p)          (p)                     
#define   Right(p)         (((p) + 1) % num_philosophers)
				       
// declare functions that are defined in this file

static int setup(void);
void philosopher(int me);
int is_odd(int me);
void eat(int me);
void leave_table(int me);
void pick_up_chopsticks(int me);
void put_down_chopsticks(int me);
void print_stomach(int rice, int num_created_philosophers);
static void eat_all(void);
static void clean_exit(int n);

void init_random();
int duration(int min, int max);


/* SHARED DATA STRUCTURES */

static int *rice=NULL;		// This is the rice on the plate

static int* stomach=NULL;	// An array of stomachs used to book
				// keep the eating.

/* SEMAPHORES */

static semid_t *chopstick_sem =NULL;	// An array of semaphores for
					// the chopsticks

static semid_t rice_sem=NULL;		// One single semaphore for
					// the rice.


int main(int argc, char **argv)
{

  int num_created_philosophers = 0;
  int   i;
  int save_rice=0;
  
 
  /* ignore ctr-c during setup */
  signal(SIGINT,SIG_IGN);

  // Check if one argument is given, if not use default
  if (argc > 1) num_philosophers=atoi(argv[1]);
  
  if (argc > 2) {
    i=atoi(argv[2]);
    
  } else {
    i=DELFAULT_RICE;
  }
  
  save_rice=i;
  
  printf("\n**********************************************\n");
  printf(" %d philosophers are in the room\n\n",num_philosophers);
  printf(" There is %d yummy bites of rice on the plate\n",i);
  printf("**********************************************\n\n");


  // Create shared memory and semaphores. Create the philosophers.
  num_created_philosophers = setup();

  
  
  // Check if two arguments is given, if not use default
  if (argc > 2) {
    *rice=atoi(argv[2]);
  } else {
    *rice=DELFAULT_RICE;
  }
  
   // signal handler for ctrl-c
  signal(SIGINT,(void*) eat_all);

  printf("\n*** %d philosophers successfully created ***\n\n",
	 num_created_philosophers);
  printf("\n*** Now I must wait for the philosophers to finish  ***\n\n");

  // wait for all philosophers to finish

  for (i=0; i < num_created_philosophers; i++) 
    printf("\n*** One philosopher finished (pid=%d) ***\n\n",(int)wait(NULL));
  
  printf("\n***********************************************\n");
  printf("*** All philosophers have left the building ***\n");
  printf("***********************************************\n\n");
  
  print_stomach(save_rice, num_created_philosophers);
  
  clean_exit(0);
  return 0;
}

/*----------------------------------------------------------------------------

  Allocate all shared memory and semaphores, then create all the
  philosophers processes

----------------------------------------------------------------------------*/
static int setup(void) {

  int num_created_philosophers = 0;
  int i;
  int pid;
  int the_cheating = 0;

  if ((rice=shmalloc(sizeof(*rice))) == NULL) {
    fprintf(stderr,"Couldn't allocate shared memory for the rice, exiting\n");
    clean_exit(1);
  }
  
  
  if ((int)(rice_sem=semcreate(1)) == -1) {
    fprintf(stderr,"Couldn't allocate semaphore for the rice, exiting\n");
    clean_exit(1);
  }
  
  if ((stomach=shmalloc(num_philosophers*sizeof(int))) == NULL) {
    fprintf(stderr,"Couldn't allocate shared memory for the stomach, exiting\n");
    clean_exit(1);
  }
  
  chopstick_sem = malloc(num_philosophers*(sizeof(semid_t)));

  for (i = 0; i < num_philosophers; ++i) {  
    stomach[i] = 0;
    if ((chopstick_sem[i] = semcreate(1))==-1) {
      fprintf(stderr,"Couldn't allocate chopstik_sem[%d], exiting\n",i);
      clean_exit(1); 
    }
  }



#ifdef CHEATING
  the_cheating = 1;
#endif

  /* create the philosophers */

  for (i = 0; i < num_philosophers - the_cheating; ++i) {
    
    pid =fork(); 

    switch (pid){

    case -1:
      
	perror("Couldn't fork");
	fprintf(stderr,"Failed to create philosopher %d\n",i);
	clean_exit(1);
	break;
    case 0:
      printf("Philosopher %d sits down at the table (pid=%d)\n",
	     i,(int)getpid());
  
      init_random();
      philosopher(i); // the philosophers goes in this function
      return 0;

    default:
      num_created_philosophers++;
    }
  }

  return num_created_philosophers;
}

 

/*----------------------------------------------------------------------------

  The philosophers processes.

  Input me:	the philosopher number



  A philosopher repeats the cycle
  
	- trie to grap a pair of chopsticks
	- eat
	- put down the chopsticks
	- think

----------------------------------------------------------------------------*/

void philosopher(int me)
{
 
   while(1) {    

     // Make one philosopher slower than the rest to make starvation
     // more likely.

     if (me==0)  sleep(SLOW); 

     pick_up_chopsticks(me);
     
     eat(me);
     
     put_down_chopsticks(me);

     printf("Philosopher %d thinks for a while.\n", me);

     THINKING();
     printf("Philosopher %d finished thinking.\n", me);
   }
}

/*----------------------------------------------------------------------------

  Input me:	the philosopher number

  Retuns:	1 if me is odd
		0 if me is even

----------------------------------------------------------------------------*/

int is_odd(int me) {
  
  return (me & 1);	// Bitwise and, the last bit in me is one iff
			// me is odd.
}

/*----------------------------------------------------------------------------

  Take one bite of rice from the plate. 

  Input me:	the philosopher number

----------------------------------------------------------------------------*/

void eat(int me)
{
  
  /**********************************************
   **********************************************
   ** YOU MUST ADD APPROPIATE SYNCHRONIZATION  **
   **********************************************
   **********************************************/
  
  char   *s;
  

  
  // If the plate is empty leave the table, otherwies take one bite of rice
  // from the plate.
  
  if (*rice==0) {
    leave_table(me);
  }
  else {
    

    *rice=*rice -1;	// Take a bite!

    
    stomach[me]++;

    s = stomach[me] == 1 ? "st" : stomach[me] == 2 ? "nd" : 
      stomach[me] == 3 ? "rd" : "th";

    printf("Philosopher %d start to eat one bite of rice for the ",me);
    printf("%i%s time (%d bites left).\n",stomach[me],s,*rice);
    fflush(stdout);

    EATING();	// Eating takes time

    printf("Philosopher %d finished eating\n",me);

    
  }
    
  
}


/*----------------------------------------------------------------------------

  Leave the table

  Input me:	the philosopher number

----------------------------------------------------------------------------*/

void leave_table(int me)
{
  put_down_chopsticks(me);
  printf("Philosopher %d finds the plate emtpy and leaves the table.\n", me);
  
  // It takes time to leave the table
  LEAVING();

  exit(0);
}


/*----------------------------------------------------------------------------

  Acquire chopsticks.

  Input me:	the philosopher number

----------------------------------------------------------------------------*/
void pick_up_chopsticks(int me)
{

  /**********************************************
   **********************************************
   ** YOU MUST ADD APPROPIATE SYNCHRONIZATION  **
   **********************************************
   **********************************************/
  
  // use chopstick_sem[Right(me)] and chopstick_sem[Left(me)] to get
  // the semaphores for the chopsticks
  
  // You may want to use is_od(me) to determine if the philosopher is
  // odd or even
  
  // simulate slow picking up to encourage deadlock
  PICK_UP();
  printf("Philosopher %d picks up left chopstick\n", me);

  // simulate slow picking up to encourage deadlock
  PICK_UP();
  printf("Philosopher %d picks up right chopstick\n", me);
  
}


/*----------------------------------------------------------------------------

  Relinquish chopsticks

  Input me:	the philosopher number

----------------------------------------------------------------------------*/

void put_down_chopsticks(int me)
{

  /**********************************************
   **********************************************
   ** YOU MUST ADD APPROPIATE SYNCHRONIZATION  **
   **********************************************
   **********************************************/
  
  
  
  if (drand48() < 0.5) {
    
    // About half the times, put down left chopstick first.
    
    printf("Philosopher %d put down left chopstick\n", me);

    PUT_DOWN();	// simulate slow put down encourage deadlock and  race
		// conditions


    
    printf("Philosopher %d put down right chopstick\n", me);
    PUT_DOWN();
  }
  else
    {
      // About half the times, put down right chopstick first.
      
    }
}



/*----------------------------------------------------------------------------

  Prints a summary of the dinner: the amount of rice for each
  philosopher is reported. If one or more philosopher doesn't get any
  rice, a starvation warning is given.
  
  INPUT:	

  rice:	the amount of rice on the plate when the dinner starts.
  
  n:	number of seated philosophers.
		

----------------------------------------------------------------------------*/

void print_stomach(int start_rice,int n) {

  int i;

  int starvation = 0;
  
  printf("\n**********************************************\n\n");
  printf("SUMMARY:\n\n");

  printf("%d philosophers at the table\n\n",n);
  printf("%d bites of rice on the plate\n\n",start_rice);
 
  
  for (i=0; i <  n; i++) {
    if (stomach[i]==0) starvation=1;
    printf("Philosopher %d had %d bites of rice\n",i,stomach[i]);
    
  }
  
  if (starvation) printf("\nOoopps, STARVATION!!!!!\n");

  if ( *rice < 0) printf("\nOoopps, negative amount of rice!!!!!\n");

  printf("\n**********************************************\n\n");
}


/*----------------------------------------------------------------------------

  Release ipc objects and exit.

  Input n:	the exit status.

----------------------------------------------------------------------------*/

static void clean_exit(int n)
{
  int i;

  if (rice) shfree(rice);
  
  if (stomach) shfree(stomach);


  if (rice_sem != -1) semdestroy(rice_sem);
  
  if (chopstick_sem) {
    for (i=0;i<num_philosophers;i++)     
      if (chopstick_sem[i] != -1) semdestroy(chopstick_sem[i]);
    
  }

  exit(n);
}

/*----------------------------------------------------------------------------

  Signal handler for the SIGINT signal (ctr-c)


  Eats all rice on the plate, the philosopher will detect this when
  they try to eat and quit.

----------------------------------------------------------------------------*/

static void eat_all(void) 
{

  semwait(rice_sem);

  *rice = 0;
  printf("**** BIG BROTHER EATS ALL RICE **** \n");
    
  semsignal(rice_sem);
  
}


/*----------------------------------------------------------------------------

  Initialize the random number generator

 ----------------------------------------------------------------------------*/
void init_random() {
        
      // Use time in seconds and pid to set seed the random generator.
      
      time_t t1;

      (void) time(&t1);
      srand48((long) t1+getpid()); 

}

/*----------------------------------------------------------------------------

  Returns a number between min and max.

 ----------------------------------------------------------------------------*/
int duration(int min, int max) {
  return (int)  (min + drand48()*(max - min));
}

/*-------------------------  END OF FILE  ----------------------------------*/
  
