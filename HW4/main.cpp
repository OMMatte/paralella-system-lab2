/* unisex bathroom solver
 
 features: uses a barrier; the Worker[0] computes
 the total sum, minumum element value and maxmimum
 element value from partial values computed by Workers
 and prints the total sum to the standard output
 
 usage under Linux:
 g++ main.cpp -o main -lpthread
 main [size] [numWorkers]
 
 */
#ifndef _REENTRANT
#define _REENTRANT
#endif
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>
#include <semaphore.h>
#include <string.h>
#include <iostream>
#include <errno.h>
#include <unistd.h>
#include <sstream>

#define SHARED 1 /* used for sem_init() to set sharing to process level */

/* index and number representation for a man and woman */
#define MAN        0
#define WOMAN      1

/* standard amount of men and woman working and going on toilet breaks */
#define NR_MEN      20
#define NR_WOMAN    30

#define MAX_WAITING_TIME 20 /* standard maxWaitingTime */
#define MAX_WORK_TIME   40  /* standard maxWorkTime */
#define MAX_TOILET_TIME 8   /* standard maxToiletTime */

#define WORKING_DAY 60     /* standard workingDay */

double longestQueuingStartTime[2]; /* if at least one person of a specific gender is queuing, the arrival time for the first arrived person of that gender that is standing in the queue is stored here. If noone is queuing of a specific gender then the value stored is undefined */
std::string genderName[2] =  {"Man", "Woman"}; /* just a textual representation of a man and woman for printing purposes */

bool exitProgram = false; /* indicates when the program should exit, aka everyone goes home for the day */

sem_t critSection;      /* semaphore used for allowing a person to come to the bathroom and leave back to the workplace */
sem_t queuedAllowed[2]; /* semaphore used for allowing entrance to the bathroom for a queued person of a specific gender */

u_int nrQueuing[2] = {0};    /* number of persons in queue for each gender */
u_int nrInBathroom[2] = {0}; /* number of persons in the bathroom for each gender */

u_int maxWaitingTime;   /* maximum waiting time, in seconds, for a person before a bathroom switch must be carried out. However, the actual waiting time might be up to maxWaitingTime + maxToiletTime since it takes some time for the other gender to finish whatever they are doing in the bathroom */
u_int maxWorkTime;      /* maximum time, in seconds, that a person works before going to the bathroom */
u_int maxToiletTime;    /* maximum time, in seconds, spent in the bathroom */
u_int workingDay;       /* how long a working day is, the program will begin exiting after this time has expired. However more time will pass since each worker will continue to work until a final visit to the bathroom has been made */

/* timer */
double read_timer() {
    static bool initialized = false;
    static struct timeval start;
    struct timeval end;
    if( !initialized )
    {
        gettimeofday( &start, NULL );
        initialized = true;
    }
    gettimeofday( &end, NULL );
    return (end.tv_sec - start.tv_sec) + 1.0e-6 * (end.tv_usec - start.tv_usec);
}

void *Person(void *);

/* read command line, initialize, and create threads */
int main(int argc, char *argv[]) {
    
    u_int numMen, numWoman;
    
    /* read command line args if any */
    numMen = (argc > 1)? atoi(argv[1]) : NR_MEN;
    numWoman = (argc > 2)? atoi(argv[2]) : NR_WOMAN;
    maxWaitingTime = (argc > 3)? atoi(argv[3]) : MAX_WAITING_TIME;
    maxWorkTime = (argc > 4)? atoi(argv[4]) : MAX_WORK_TIME;
    maxToiletTime = (argc > 5)? atoi(argv[5]) : MAX_TOILET_TIME;
    workingDay = (argc > 6)? atoi(argv[6]) : WORKING_DAY;
    
    pthread_t men[numMen];
    pthread_t woman[numWoman];
    
    /* set global thread attributes */
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
    
    /* initialize the semaphores */
    /* only critSection is set to 1 since the first thing that will happen is that a thread reaches a critical section */
    for(int i = 0; i < 2; i++){
        if(sem_init(&queuedAllowed[i], SHARED, 0) == -1) {
            printf("sem_init for queuedAllowed %d: failed: %s\n", i, strerror(errno));
            return -1;
        }
    }
    if(sem_init(&critSection, SHARED, 1) == -1) {
        printf("sem_init for critsection: failed: %s\n", strerror(errno));
        return -1;
    }
    
    double workStartTime = read_timer(); /* the starting time for todays work */
    double workEndTime = workStartTime + workingDay; /* the time everyone get off work */
    
    /* create the threads representing men and woman */
    for (int i = 0; i < numMen; i++)
        pthread_create(&men[i], &attr, Person, (void *) (long)MAN);
    
    for (int i = 0; i < numWoman; i++)
        pthread_create(&woman[i], &attr, Person, (void *) (long)WOMAN);
    
    
    while(true){
        /* the main thread checks if the working day is over and calls everyone home before 'closing the business'*/
        if(read_timer() > workEndTime) {
            /* time to get off! set the exit boolean so that the men and woman (threads) can exit */
            printf("Time to quit! Go home after your last stop in the bathroom. Time: %f\n", read_timer() - workStartTime);
            exitProgram = true;
            break;
        } else {
            /* the day has not ended yet, yield the time */
            pthread_yield();
        }
    }
    /* we wait for the workers to leave work before closing */
    for (int i = 0; i < numMen; i++)
        pthread_join(men[i], NULL);
    
    for (int i = 0; i < numMen; i++)
        pthread_join(woman[i], NULL);
    
    printf("Good bye! Hope everyone had a great day. Total time from opening until last person left the work place: %f\n", read_timer() - workStartTime);
    
    pthread_exit(NULL);
}

/* helping printing functions */
void printInfo(const std::string & prefix, long gender, double timeTaken) {
    printf("%s %s. Time taken: %f. Queuing (M/W) & in bathroom (M/W): (%d/%d), (%d/%d)\n" , genderName[gender].c_str(), prefix.c_str(), timeTaken, nrQueuing[MAN], nrQueuing[WOMAN], nrInBathroom[MAN], nrInBathroom[WOMAN]);
}
void printInfo(const std::string & prefix, long gender) {
    printf("%s %s. Queuing (M/W) & in bathroom (M/W): (%d/%d), (%d/%d)\n" , genderName[gender].c_str(), prefix.c_str(), nrQueuing[MAN], nrQueuing[WOMAN], nrInBathroom[MAN], nrInBathroom[WOMAN]);
}

/* Each worker sums the values in one strip of the matrix.
 After a barrier, worker(0) computes and prints the total */
void *Person(void *arg) {
    long myGender = (long) arg;
    long otherGender = (myGender + 1) % 2;
    double cameToBathroomTime;
    double enteredBathroomTime;
    
    while(!exitProgram){
        /* begin with working (sleeping) */
        usleep((rand() % (maxWorkTime * 1000)) * 1000);
        /* wait until you can enter critical section, in this case meaning going to the bathroom */
        sem_wait(&critSection);
        cameToBathroomTime = read_timer();
        
        if(nrInBathroom[otherGender] > 0 ||
           /* the other gender is inside the bathroom, you have to stand in queue! */
                (nrQueuing[otherGender] > 0 && cameToBathroomTime - longestQueuingStartTime[otherGender] > maxWaitingTime &&
                 /* the other gender have been waiting to long in queue, so you have to wait in quoue */
                    (nrQueuing[myGender] == 0 || longestQueuingStartTime[otherGender] < longestQueuingStartTime[myGender] )))
                    /* but we also have to check so that our gender have not been waiting even longer, in that case go ahead and use the bathroom instead */
        {
            /* we have to stand in the quoue to the bathroom */
            if(nrQueuing[myGender] == 0) {
                /* if i am first in line of my gender, mark the time of my arrival so my gender gets to use the bathroom if the other gender are taking to much time */
                longestQueuingStartTime[myGender] = read_timer();
            }
            nrQueuing[myGender] ++;
            printInfo("started queuing", myGender);
            sem_post(&critSection);
            /* we wait until we our gender is allowed to enter */
            sem_wait(&queuedAllowed[myGender]);
            nrQueuing[myGender] --;
        }
        /* we have entered the bathroom! */
        nrInBathroom[myGender] ++;
        if(nrQueuing[myGender] > 0) {
            /* make sure the let everyone in the quoue of my gender enter the bathroom aswell */
            sem_post(&queuedAllowed[myGender]);
        } else {
            /* if we have noone in queue of my gender we open up the critical section instead and let other come to and leave the bathroom */
            sem_post(&critSection);
        }
        
        enteredBathroomTime = read_timer();
        printInfo("entered bathroom", myGender, enteredBathroomTime - cameToBathroomTime);
        /* we spend some time in the bathrrom */
        usleep((rand() % (maxToiletTime*1000)) * 1000);
        /* we have to wait since we are entiring a critical section when leaving the bathroom */
        sem_wait(&critSection);
        nrInBathroom[myGender] --;
        printInfo("left bathroom", myGender, read_timer() - enteredBathroomTime);
        
        if(nrInBathroom[myGender] > 0) {
            /* We still have people of my gender in the bathroom, open the critical section so they can leave */
            sem_post(&critSection);
        } else {
            if(nrQueuing[otherGender] > 0) {
                /* we have noone in the bathroom and since my gender was there lastly we let the other gender enter */
                sem_post(&queuedAllowed[otherGender]);
            } else {
                /* we have noone in the bathrrom and noone queing, we just open the critical section to let people come to the bathroom */
                sem_post(&critSection);
            }
        }
    }
    pthread_exit(NULL);
}
