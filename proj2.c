/**
 * FILE:    proj2.c
 * AUTHOR:  Vojtech Kalis, xkalis03@fit.vutbr.cz
 * NAME:    Project #2 for IOS (Operation Systems) VUT FIT
 * DATE:    28.04.2021
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <signal.h>

#define shmem_key 0x6543210 //id key for shared memory

///'help' message, pretty self-explanatory
const char *helpmsg =
    " There was an error processing the values entered from stdin, please ensure they meet the following format.\n"
    "\n"
    " Usage:   ./proj2 NE NR TE TR   where:\n"
    "\n"
    "   NE    -number of elves\n"
    "         -has to be in range:  0 < NE < 1000\n"
    "   NR    -number of reindeers\n"
    "         -has to be in range:  0 < NE < 20\n"
    "   TE    -the maximum time (in miliseconds) elves can spend working alone (without the need of Santa's help)\n"
    "         -has to be in range:  0 <= TE <= 1000\n"
    "   TR    -the maximum time (in miliseconds) reindeers can spend on their holidays\n"
    "         -has to be in range:  0 <= TR <= 1000\n"
    "\n"
    " All arguments have to be whole numbers."
    "\n";

/********************************
 *                              *
 *        SHARED MEMORY         *
 *                              *
 *******************************/ 
typedef struct {
    int action;             // action counter
    int reindeersReady;     // counter for reindeers who returned from holiday
    int reindeersHitched;   // counter for reindeers hitched to Santa's sleigh
    int inFrontOfWorkshop;  // counter for elves waiting in front of workshop
    int doorSign;           // before Santa hitches up all reindeers, he pust a sign on the workshop which reads 
                            //   "Christmas - closed!", signalling all elves that they can go on holiday
    int helpedElves;        // counter used by elves receiving help by Santa, so the last elf can set the 
                            //   'elvesHelpAcknowledged' semaphore

    sem_t writing;          // used by all processes to prevent simultaneous writing into the .out file
    sem_t elfMutex;         // used by elves to prevent other elves from entering the workshop while they're being 
                            //   helped by Santa
    sem_t elfHelped;        // semaphore used by elves to signal they received help
    sem_t elvesHelpAcknowledged; // semaphore used by elves in workshop to tell Santa that all of them received help
    sem_t reindeerMutex;    // mutex used by reindeers to prevent more than one to increment the 'reindeersReady' counter 
	sem_t mutex;            // used by all processes to control execution (and prevent errors resulting from simultaneous 
                            //   reaching into shared memory)
	sem_t santaSem;         // semaphore used by reindeers and elves to wake up Santa
    sem_t getHitched;       // set by Santa and used by reindeers to get hitched to Santa's sleigh
    sem_t allHitched;       // set by the last reindeer to signal Santa that all reindeers have been hitched to the sleigh, 
                            //   and Christmas can start!
} sharedMem;


/********************************
 *                              *
 *       GLOBAL VARIABLES       *
 *                              *
 *******************************/ 
int NE = 0; //input argv[0] for number of elves
int NR = 0; //input argv[1] for number of reindeers
int TE = 0; //input argv[2] for maximal time delay for elves to spend working alone
int TR = 0; //input argv[3] for maximal time delay for reindeers to spend on holiday

sharedMem *shmem; //shared memory segment pointed

FILE *fp; //file pointer

/********************************
 *                              *
 *          FUNCTIONS           *
 *                              *
 *******************************/

/**
 *  argsCheck
 * -----------------------------------------------
 *  @brief: checks if input arguments meet required format
 *  
 *  @param argc: standard input argument count 
 *  @param argv: standard input argument values
 */
void argsCheck(int argc, char* argv[]){
    long val;
    char *next;
	if(argc!=5){ //test for number of arguments and floats/non-number characters
        fprintf(stderr,"%s",helpmsg);
        exit(1);
    }
    for (int i = 1; i < argc; i++) { //process all arguments one-by-one
        val = strtol (argv[i], &next, 10); //get value of arguments, stopping when NoN encountered
        if ((val%1 != 0) || (next == argv[i]) || (*next != '\0')) { // Check for empty string and characters left after conversion
            fprintf(stderr,"%s",helpmsg);
            exit(1);
        }
    }
}

/**
 *  argsLoad
 * -----------------------------------------------
 *  @brief: loads and stores arguments from standard input into pre-prepared global variables
 *  
 *  @param argv: standard input argument values
 */
void argsLoad(char* argv[]){
    NE = atof(argv[1]);
	NR = atof(argv[2]);
	TE = atof(argv[3]);
	TR = atof(argv[4]);
    if ((NE <= 0 || NE >= 1000)|| //0 < NE < 1000
        (NR <= 0 || NR >= 20)  || //0 < NR < 20
        (TE < 0 || TE > 1000)  || //0 <= TE <= 1000
        (TR < 0 || TR > 1000))    //0 <= TR <= 1000
	{
		fprintf(stderr,"%s",helpmsg); /// print help
		exit(1);
	}
}

/**
 *  freeShmem
 * -----------------------------------------------
 *  @brief: frees shared memory
 *  
 *  @param shmem: pointer to the shared memory structure
 *  @param id: identification number of the shared memory segment to be cleared
 */
void freeShmem(sharedMem *shmem, int id){
    shmdt(shmem); //detaches shared memory
    shmctl(id, IPC_RMID, NULL); //sets it to be deleted
}

/**
 *  initSemaphores
 * -----------------------------------------------
 *  @brief: initializes semaphores and counters in shared memory to wanted values
 *  
 *  @param shmem: pointer to the shared memory structure 
 *  @param id: identification number of the shared memory segment to be cleared
 */
void initSemaphores(sharedMem* shmem, int id){
    bool err = false;

    if (sem_init(&(shmem->writing),1,1) < 0) {err = true;};
    if (sem_init(&(shmem->elfMutex),1,1) < 0) {err = true;};
    if (sem_init(&(shmem->elfHelped),1,0) < 0) {err = true;};
    if (sem_init(&(shmem->elvesHelpAcknowledged),1,0) < 0) {err = true;};
    if (sem_init(&(shmem->reindeerMutex),1,1) < 0) {err = true;};
    if (sem_init(&(shmem->mutex),1,1) < 0) {err = true;};
    if (sem_init(&(shmem->santaSem),1,0) < 0) {err = true;};
    if (sem_init(&(shmem->getHitched),1,0) < 0) {err = true;};
    if (sem_init(&(shmem->allHitched),1,0) < 0) {err = true;};

    shmem->helpedElves = 0;
    shmem->action = 1;
    shmem->reindeersHitched = 0;
    shmem->reindeersReady = 0;
    shmem->inFrontOfWorkshop = 0;
    shmem->doorSign = 0;

    if (err == true){
        fprintf(stderr,"Error initializing semaphores\n");
        freeShmem(shmem,id);
        exit (1);
    }
}

/**
 *  santaFunc
 * -----------------------------------------------
 *  @brief: function for child process Santa
 *  
 *  @param shmem: pointer to the shared memory structure
 */
void santaFunc(sharedMem* shmem){
    while(1){
        sem_wait(&(shmem->writing));
            fprintf(fp,"%d: Santa: going to sleep\n",shmem->action++);
            fflush(fp); //forces immediate writing
        sem_post(&(shmem->writing));

        sem_wait(&(shmem->santaSem)); //Santa waits until he's woken up

        sem_wait(&(shmem->mutex));
            if (shmem->reindeersReady == NR){ //Santa woken up by reindeers
                sem_wait(&(shmem->writing));
                    fprintf(fp,"%d: Santa: closing workshop\n",shmem->action++);
                    fflush(fp); //forces immediate writing
                sem_post(&(shmem->writing));

                shmem->doorSign = 1; //close workshop

                for (int j = 0; j < NE; j++){   //let all elves go on holiday
                    sem_post(&(shmem->elfHelped));
                }

                for (int i = 0; i < NR; i++){   //signal all reindeers to get hitched
                    sem_post(&(shmem->getHitched));
                }

                sem_post(&(shmem->mutex));
                sem_wait(&(shmem->allHitched)); //wait for all reindeers to get hitched

                sem_wait(&(shmem->writing));
                    fprintf(fp,"%d: Santa: Christmas started\n",shmem->action++);
                    fflush(fp); //forces immediate writing
                sem_post(&(shmem->writing));                

                exit(0); //all done, exit process

            } else if (shmem->inFrontOfWorkshop == 3){ //Santa woken up by elves
                shmem->inFrontOfWorkshop = 0;
                sem_wait(&(shmem->writing));
                    fprintf(fp,"%d: Santa: helping elves\n",shmem->action++);
                    fflush(fp); //forces immediate writing
                sem_post(&(shmem->writing));

                for (int i = 0; i < 3; i++){ //let the three elves inside workshop to get out of wait
                    sem_post(&(shmem->elfHelped));
                }

                sem_post(&(shmem->mutex));
                sem_wait(&(shmem->elvesHelpAcknowledged)); //wait until the three elves inside workshop leave
            }
    }
}

/**
 *  elfFunc
 * -----------------------------------------------
 *  @brief: function for elf child processes
 *  
 *  @param num: elfID, elf's identification number
 *  @param shmem: pointer to the shared memory structure 
 */
void elfFunc(int num, sharedMem* shmem){
    sem_wait(&(shmem->writing));
        fprintf(fp,"%d: Elf %d: started\n",shmem->action++, num);
        fflush(fp); //forces immediate writing
    sem_post(&(shmem->writing));

    while(1){
        usleep(rand() % (TE + 1) * 1000); //interval <0,TE>, simulates elf's working alone
        if (shmem->doorSign != 1){        //if workshop's not yet closed
            sem_wait(&(shmem->writing));
                fprintf(fp,"%d: Elf %d: need help\n",shmem->action++, num);
                fflush(fp); //forces immediate writing
            sem_post(&(shmem->writing));
        }

        sem_wait(&(shmem->elfMutex)); //if there aren't three elves in queue yet, enter queue
        sem_wait(&(shmem->mutex));

        shmem->inFrontOfWorkshop++;   //elf's waiting in front of workshop
        if (shmem->inFrontOfWorkshop == 3){ //if elf's the third elf in workshop
            sem_post(&(shmem->santaSem));   //signal Santa to come and help
        } else {                            //else
            sem_post(&(shmem->elfMutex));   //let another elf enter queue
        }

        sem_post(&(shmem->mutex));
        sem_wait(&(shmem->elfHelped)); //wait to receive help by Santa
        sem_wait(&(shmem->mutex));

        if (shmem->doorSign != 1){     //if workshop's not yet closed
            sem_wait(&(shmem->writing));
                fprintf(fp,"%d: Elf %d: get help\n",shmem->action++, num);
                fflush(fp); //forces immediate writing
            sem_post(&(shmem->writing));
            shmem->helpedElves++;      //elf received help
            if (shmem->helpedElves == 3){ //if elf's the last elf to get help
                shmem->helpedElves = 0;
                sem_post(&(shmem->elvesHelpAcknowledged)); //tell Santa all's good
                sem_post(&(shmem->elfMutex)); //restard queue
            }
            sem_post(&(shmem->mutex));
        } else { //if workshop is closed and elf is in queue
            sem_wait(&(shmem->writing));
                fprintf(fp,"%d: Elf %d: taking holidays\n",shmem->action++, num);
                fflush(fp); //forces immediate writing
            sem_post(&(shmem->writing));
            shmem->helpedElves++;
            if (shmem->helpedElves == 3){
                shmem->helpedElves = 0;
                sem_post(&(shmem->elvesHelpAcknowledged));
                sem_post(&(shmem->elfMutex));
            }
            sem_post(&(shmem->mutex));
            exit(0); //all done, exit process
        }
    }
}

/**
 *  reindeerFunc
 * -----------------------------------------------
 *  @brief: function for reindeer child processes
 *  
 *  @param num: rdID, reindeer's identification number
 *  @param shmem: pointer to the shared memory structure 
 */
void reindeerFunc(int num, sharedMem* shmem){
    sem_wait(&(shmem->writing));
        fprintf(fp,"%d: RD %d: rstarted\n",shmem->action++, num);
        fflush(fp); //forces immediate writing
    sem_post(&(shmem->writing));

    usleep((rand() %(TR/2 + 1) + TR/2) * 1000); //interval <TR/2,TR>, simulates reindeer's holiday length

    sem_wait(&(shmem->writing));
        fprintf(fp,"%d: RD %d: return home\n",shmem->action++, num);
        fflush(fp); //forces immediate writing
    sem_post(&(shmem->writing));

    sem_wait(&(shmem->reindeerMutex));
        shmem->reindeersReady++; //increment counter of reindeers ready to be hitched
    sem_post(&(shmem->reindeerMutex));

    if (shmem->reindeersReady == NR){ //if reindeer's the last one to return from holiday
        sem_post(&(shmem->santaSem)); //wake up Santa
    }

    sem_wait(&(shmem->getHitched)); //wait for Santa to signal reindeers to get hitched
    sem_wait(&(shmem->writing));
        shmem->reindeersHitched++;
        fprintf(fp,"%d: RD %d: get hitched\n",shmem->action++, num);
        fflush(fp); //forces immediate writing
    sem_post(&(shmem->writing));

    if (shmem->reindeersHitched == NR){ //if reindeers's the last one to be hitched
        sem_post(&(shmem->allHitched)); //signal Santa the sleigh is ready
    }

    exit(0); //all done, exit process
}

/********************************
 *                              *
 *            MAIN              *
 *                              *
 *******************************/ 
int main(int argc, char *argv[])
{
/*Storing values from standard input. All parametres need to be given as whole numbers in their 
  respective ranges, else the program prints the help message to standard error output and exits.*/
    argsCheck(argc, argv);
//Values okay, load and save them
    argsLoad(argv);
//Values read successfully, open file 'proj2.out' for writing
    fp = fopen("proj2.out", "w");
//Use current time as seed for random number generator
    srand(time(0));
//Segment which initializes shared memory
    int id;
    id = shmget(shmem_key, 20 * sizeof(sharedMem), IPC_CREAT | 0644);
    shmem = (sharedMem *) shmat(id, NULL, 0);
//Initialize shared memory structure
    initSemaphores(shmem, id);

///Create child processes for Santa, Elves and Reindeers
    int idS = fork();
    if (idS == -1) { //forking error occurred
        fprintf(stderr,"Fork error for Santa\n");
        freeShmem(shmem,id);
        exit (1);
    } else if (idS == 0) {
        santaFunc(shmem); //Santa child process enters its function, at the end of which it kills itself
    }
    
    int more; //determine whether there's more elves or reindeers
    if (NE > NR){
        more = NE;
    } else {
        more = NR;
    }
    
    for (int i = 1, j = 1, l = 1; l <= more+1; i++, j++, l++) {
        if (i < NE+1){ //create elf
            int elfID = fork();
            if (elfID == -1) { //forking error occurred
                fprintf(stderr,"Fork error for elf #%d\n", i);
                freeShmem(shmem,id);
                exit (1);
            } else if (elfID == 0) { //if we're a child process
                elfFunc(i, shmem); //elf child process enters its function, at the end of which it kills itself
            }
        }
        if (j < NR+1){ //create reindeer
            int rdID = fork();
            if (rdID == -1){ //forking error occurred
                fprintf(stderr,"Fork error for reindeer #%d\n", j);
                freeShmem(shmem,id);
                exit (1);
            } else if (rdID == 0) { //if we're a child process
                reindeerFunc(j, shmem);//reindeer child process enters its function, at the end of which it kills itself
            }
        }
    }

//Wait for all child processes to die
    while (wait(NULL) != -1 || errno != ECHILD);
//Release and clear the shared memory
    freeShmem(shmem,id);
    fclose(fp);
//auxiliary print just for checking args (comment out in finished project)
    //printf("\nNE = %d\nNR = %d\nTE = %d\nTR = %d\n",NE,NR,TE,TR);
    
    return 0;
}