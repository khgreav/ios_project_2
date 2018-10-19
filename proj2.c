/*
 * File:    proj2.c
 * Author:  xhanak34
 *
 * Created on April 23, 2018, 10:10 AM
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/shm.h>

//defines
#define MUTEX_SEM "/xhanak34.mutex"
#define BUS_SEM "/xhanak34.bus"
#define BOARDED_SEM "/xhanak34.boarded"
#define RIDE_SEM "/xhanak34.ride"
#define LOGGER_SEM "/xhanak34.logger"
#define FILENAME "proj2.out"

//global variables
sem_t *mutex = NULL; //protects riders - mutually exclusive
sem_t *bus = NULL; //bus control
sem_t *boarded = NULL; //boarding signal
sem_t *ride = NULL; //end of ride signal
sem_t *logger = NULL; //allows logging
FILE *logfile; //logfile - proj2.out
int *waiting = NULL; //number of riders waiting at a station
int *lognum = NULL; //number of log entry
int *traveled = NULL; //number of riders traveled ()
int waitingID = 0; //id for waiting shm
int lognumID = 0; //id for lognum shm
int traveledID = 0; //id for traveled shm

//function prototypes
void initResources();
void cleanResources();
void busProcess(int r, int c, int abt);
void createRider(int r, int art);
void rider(int n);

/**
 * Function initResources creates and initializes shared memory, semaphores
 * and log file. Called at the beginning of the program.
 */
void initResources(){
    //create shared memory
    waitingID = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0777);
    lognumID = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0777);
    traveledID = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0777);

    //check for shared memory errors
    if(waitingID == -1 || lognumID == -1 || traveledID == -1){
        fprintf(stderr, "Could not create shared memory.\n");
        cleanResources();
        exit(1);
    }

    //attach memory
    waiting = shmat(waitingID, NULL, 0);
    lognum = shmat(lognumID, NULL, 0);
    traveled = shmat(traveledID, NULL, 0);

    //check for memory attachment error
    if(waiting == NULL || lognum == NULL || traveled == NULL){
        fprintf(stderr, "Could not attach memory to pointer.\n");
        cleanResources();
        exit(1);
    }
    
    //initial shared memory values
    *waiting = 0;
    *lognum = 1;
    *traveled = 0;

    //create and initialize semaphores
    mutex = sem_open(MUTEX_SEM, O_CREAT, 0777, 1);
    bus = sem_open(BUS_SEM, O_CREAT, 0777, 0);
    boarded = sem_open(BOARDED_SEM, O_CREAT, 0777, 0);
    ride = sem_open(RIDE_SEM, O_CREAT, 0777, 0);
    logger = sem_open(LOGGER_SEM, O_CREAT, 0777, 1);

    //check for errors
    if(mutex == SEM_FAILED || bus == SEM_FAILED || boarded == SEM_FAILED || ride == SEM_FAILED || logger == SEM_FAILED){
        fprintf(stderr, "Could not create semaphores.\n");
        cleanResources();
        exit(1);
    }
    
    //open logfile and check for errors
    logfile = fopen(FILENAME,"w");
    if(logfile == NULL){
        fprintf(stderr, "Could not create/open logfile.\n");
        cleanResources();
        exit(1);
    }
    setbuf(logfile,NULL); //prevent buffering issues, avoid flushing
}

/**
 * Function cleanResources destroys shared memory and semaphores.
 * Also closes log file. Called at the end of the program or whenever
 * an error occurs. 
 */
void cleanResources(){
    //clean-up shared memory
    shmctl(waitingID, IPC_RMID, NULL);
    shmctl(lognumID, IPC_RMID, NULL);
    shmctl(traveledID, IPC_RMID, NULL);
    
    //close and unlink semaphores to destroy them
    sem_close(mutex);
    sem_close(bus);
    sem_close(boarded);
    sem_close(ride);
    sem_close(logger);
    sem_unlink(MUTEX_SEM);
    sem_unlink(BUS_SEM);
    sem_unlink(BOARDED_SEM);
    sem_unlink(RIDE_SEM);
    sem_unlink(LOGGER_SEM);
    
    //close file
    fclose(logfile);
}

/**
 * Function busProcess represents the process of a bus in bus senate problem.
 * Bus starts, arrives at a station, let's riders board the bus and then departs.
 * Bus keeps riding, until all the riders traveled to their destination.
 * @param r number of riders, used to stop bus when all riders traveled
 * @param c bus capacity, bus can only fit c riders in for one ride
 * @param abt bus ride time
 */
void busProcess(int r, int c, int abt){
    sem_wait(logger);
    fprintf(logfile, "%d\t: BUS \t: start\n", (*lognum)++);
    sem_post(logger);
    while(*traveled < r){
        //arrival log
        sem_wait(logger);
        fprintf(logfile, "%d\t: BUS \t: arrival\n", (*lognum)++);
        sem_post(logger);

        //bus code
        sem_wait(mutex);
        if((*waiting) > 0){
            sem_wait(logger);
            fprintf(logfile, "%d\t: BUS \t: start boarding: %d\n", (*lognum)++, (*waiting));
            sem_post(logger);
        }
        int n = ((*waiting) <= c ? (*waiting): c);
        int i = 0;
        for(; i < n; i++){ //signal riders to board
            sem_post(bus);
            sem_wait(boarded);
        }
        (*waiting) = ((*waiting)-c >= 0 ? (*waiting)-c: 0);
        if(n > 0){ //riders on station, print end boarding
            sem_wait(logger);
            fprintf(logfile, "%d\t: BUS \t: end boarding: %d\n", (*lognum)++, (*waiting));
            sem_post(logger);
        }
        sem_post(mutex);

        //departure log
        sem_wait(logger);
        fprintf(logfile, "%d\t: BUS \t: depart\n", (*lognum)++);
        sem_post(logger);
        if(abt > 0){
            usleep((rand() % abt) * 1000);
        }
        sem_wait(logger);
        fprintf(logfile, "%d\t: BUS \t: end\n", (*lognum)++);
        sem_post(logger);
        for(; i > 0; i--){ //rider processes finish when bus ends a ride
            sem_post(ride);
        }
    }
    sem_wait(logger);
    fprintf(logfile, "%d\t: BUS \t: finish\n", (*lognum)++);
    sem_post(logger);
}

/**
 * Function createRider serves as a rider spawner. Rider spawner waits abt time
 * before the next rider is spawned.
 * @param r number of riders to spawn
 * @param art time before a rider is spawned
 */
void createRider(int r, int art){
    for(int i = 1; i <= r; i++){
        pid_t RID = fork();
        if(RID == 0){
            sem_wait(logger);
            fprintf(logfile, "%d\t: RID %d\t: start\n", (*lognum)++, i);
            sem_post(logger);
            rider(i);
            sem_wait(ride); //wait for bus to finish a ride
            sem_wait(logger);
            fprintf(logfile, "%d\t: RID %d\t: finish\n", (*lognum)++, i);
            sem_post(logger);
            exit(0);
        }else if(RID < 0){
            fprintf(stderr, "Rider create process error.\n");
            cleanResources();
            exit(1);
        }
        if(art > 0){
            usleep((rand() % art) * 1000);
        }
    }
    while(wait(NULL) > 0);
}

/**
 * Function rider represents rider process. Rider enters a station, boards
 * a bus once it arrives at a station, and ends when the bus ends a ride.
 * @param n rider number
 */
void rider(int n){
    //pass mutex and increment number of riders at station
    sem_wait(mutex);
    //log rider's arrival to station
    sem_wait(logger);
    fprintf(logfile, "%d\t: RID %d\t: enter: %d\n", (*lognum)++, n, ++(*waiting));
    sem_post(logger);
    sem_post(mutex);
    //boarding
    sem_wait(bus);
    //log boarding rider
    sem_wait(logger);
    fprintf(logfile, "%d\t: RID %d\t: boarding\n", (*lognum)++, n);
    (*traveled)++;
    sem_post(logger);
    sem_post(boarded);
}

/*
 *
 */
int main(int argc, char** argv) {
    int r, c, art, abt;
    char *error;

    if(argc != 5){ //check number of arguments
        fprintf(stderr, "Invalid number of arguments.\n");
        return(1);
    }else{
        r = strtol(argv[1],&error, 10);
        if(strlen(error) > 0 || r <= 0){ //check number of riders
            fprintf(stderr, "Number of riders is not valid.\n");
            return(1);
        }else{
            c = strtol(argv[2],&error, 10);
            if(strlen(error) > 0 || c <= 0){ //check bus capacity
                fprintf(stderr, "Bus capacity is not valid.\n");
                return(1);
            }else{
                art = strtol(argv[3],&error, 10);
                if(strlen(error) > 0 || art < 0 || art > 1000){ //check rider spawn delay
                    fprintf(stderr, "Rider process generation delay is not valid.\n");
                    return(1);
                }else{
                    abt = strtol(argv[4],&error, 10);
                    if(strlen(error) > 0 || abt < 0 || abt > 1000){ // check bus ride time
                        fprintf(stderr, "Bus process ride time is not valid.\n");
                        return(1);
                    }
                }
            }
        }
    }

    initResources();

    //create bus process from main process
    pid_t busID = fork();
    if(busID == 0){
        busProcess(r, c, abt);
        exit(0);
    }
    if(busID > 0){
        //create rider spawner process from main process
        pid_t riderSID = fork();
        if(riderSID == 0){
            createRider(r, art);
            return(0);
        }else if(riderSID < 0){
            fprintf(stderr, "Rider spawner process error.\n");
            cleanResources();
            return(1);
        }
    }else{
        fprintf(stderr, "Bus process error.\n");
        cleanResources();
        return(1);
    }
    cleanResources();
    return(0);
}
