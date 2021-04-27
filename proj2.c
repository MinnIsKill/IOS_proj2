/**
 * FILE:    proj2.c
 * AUTHOR:  Vojtech Kalis, xkalis03@fit.vutbr.cz
 * NAME:    Project #2 for IOS (Operation Systems) VUT FIT
 * DATE:    25.04.2021
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

#define shmem_key 0x6543210

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

/// shared memory
typedef struct {
    int action;             // action counter
    int reindeersReady;     // ready reindeers
    int reindeersHitched;   // hitched reindeers
    int inFrontOfWorkshop;  // elves in queue
    int doorSign;           // before Santa hitches up all reindeers, he pust a sign on the workshop which reads "Christmas - closed!"
    int helpedElves;

    sem_t writing;
    sem_t elfMutex;         // used by elves to prevent other elves from entering the workshop while they're being helped by Santa
    sem_t elfHelped;
    sem_t elvesHelpAcknowledged;
    sem_t reindeerMutex;
	sem_t mutex;
	sem_t santaSem;
    sem_t getHitched;
    sem_t allHitched;
} sharedMem;


/// GLOBAL VARIABLES
int NE = 0;
int NR = 0;
int TE = 0;
int TR = 0;

sharedMem *shmem;

FILE *fp;

/// FUNCTIONS

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

void freeShmem(sharedMem *shmem, int id){
    shmdt(shmem);
    shmctl(id, IPC_RMID, NULL);
}

void initSemaphores(sharedMem* shmem, int id){
    bool err = false;

    if (sem_init(&(shmem->writing),1,1) < 0) {err = true;};
    if (sem_init(&(shmem->elfMutex),1,1) < 0) {err = true;};
    if (sem_init(&(shmem->elfHelped),1,0) < 0) {err = true;};
    if (sem_init(&(shmem->elvesHelpAcknowledged),1,0) < 0) {err = true;};
    if (sem_init(&(shmem->reindeerMutex),1,1) < 0) {err = true;};
    if (sem_init(&(shmem->mutex),1,1) < 0) {err = true;};
    if (sem_init(&(shmem->santaSem),1,0) < 0) {err = true;};
    if (sem_init(&(shmem->getHitched),1,0) < 0) {err = true;}; //begin at 0, santa sem_posts NR times, all reindeers get hitched NR times (sem_wait)
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

void santaFunc(sharedMem* shmem){
    while(1){
        sem_wait(&(shmem->writing));
            fprintf(fp,"%d: Santa: going to sleep\n",shmem->action++);
            fflush(fp); //forces immediate writing
        sem_post(&(shmem->writing));

        sem_wait(&(shmem->santaSem));

        sem_wait(&(shmem->mutex));
            if (shmem->reindeersReady == NR){
                sem_wait(&(shmem->writing));
                    fprintf(fp,"%d: Santa: closing workshop\n",shmem->action++);
                    fflush(fp); //forces immediate writing
                sem_post(&(shmem->writing));

                shmem->doorSign = 1; //close workshop

                for (int j = 0; j < NE; j++){
                    sem_post(&(shmem->elfHelped));
                }

                for (int i = 0; i < NR; i++){ //release NR number of times
                    sem_post(&(shmem->getHitched));
                }

                sem_post(&(shmem->mutex));
                sem_wait(&(shmem->allHitched)); //wait for all reindeers to get hitched

                sem_wait(&(shmem->writing));
                    fprintf(fp,"%d: Santa: Christmas started\n",shmem->action++);
                    fflush(fp); //forces immediate writing
                sem_post(&(shmem->writing));                

                exit(0); //all done, exit process

            } else if (shmem->inFrontOfWorkshop == 3){
                shmem->inFrontOfWorkshop = 0;
                sem_wait(&(shmem->writing));
                    fprintf(fp,"%d: Santa: helping elves\n",shmem->action++);
                    fflush(fp); //forces immediate writing
                sem_post(&(shmem->writing));

                for (int i = 0; i < 3; i++){ //release 3 number of times
                    sem_post(&(shmem->elfHelped));
                }

                sem_post(&(shmem->mutex));
                sem_wait(&(shmem->elvesHelpAcknowledged));
            }
    }
}

void elfFunc(int num, sharedMem* shmem){
    sem_wait(&(shmem->writing));
        fprintf(fp,"%d: Elf %d: started\n",shmem->action++, num);
        fflush(fp); //forces immediate writing
    sem_post(&(shmem->writing));

    while(1){
        usleep(rand() % (TE + 1) * 1000); // interval <0,TE>
        if (shmem->doorSign != 1){
            sem_wait(&(shmem->writing));
                fprintf(fp,"%d: Elf %d: need help\n",shmem->action++, num);
                fflush(fp); //forces immediate writing
            sem_post(&(shmem->writing));
        }

        sem_wait(&(shmem->elfMutex));
        sem_wait(&(shmem->mutex));

        shmem->inFrontOfWorkshop++;
        if (shmem->inFrontOfWorkshop == 3){
            sem_post(&(shmem->santaSem));
        } else {
            sem_post(&(shmem->elfMutex));
        }

        sem_post(&(shmem->mutex));
        sem_wait(&(shmem->elfHelped));
        sem_wait(&(shmem->mutex));

        if (shmem->doorSign != 1){
            sem_wait(&(shmem->writing));
                fprintf(fp,"%d: Elf %d: get help\n",shmem->action++, num);
                fflush(fp); //forces immediate writing
            sem_post(&(shmem->writing));
            shmem->helpedElves++;
            if (shmem->helpedElves == 3){
                shmem->helpedElves = 0;
                sem_post(&(shmem->elvesHelpAcknowledged));
                sem_post(&(shmem->elfMutex));
            }
            sem_post(&(shmem->mutex));
        } else {
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
            exit(0);
        }
    }
}

void reindeerFunc(int num, sharedMem* shmem){
    sem_wait(&(shmem->writing));
        fprintf(fp,"%d: RD %d: rstarted\n",shmem->action++, num);
        fflush(fp); //forces immediate writing
    sem_post(&(shmem->writing));

    usleep((rand() %(TR/2 + 1) + TR/2) * 1000); // interval <TR/2,TR>

    sem_wait(&(shmem->writing));
        fprintf(fp,"%d: Reindeer %d: return home\n",shmem->action++, num);
        fflush(fp); //forces immediate writing
    sem_post(&(shmem->writing));

    sem_wait(&(shmem->reindeerMutex));
        shmem->reindeersReady++;
    sem_post(&(shmem->reindeerMutex));

    if (shmem->reindeersReady == NR){
        sem_post(&(shmem->santaSem));
    }

    sem_wait(&(shmem->getHitched));
    sem_wait(&(shmem->writing));
        shmem->reindeersHitched++;
        fprintf(fp,"%d: Reindeer %d: get hitched\n",shmem->action++, num);
        fflush(fp); //forces immediate writing
    sem_post(&(shmem->writing));

    if (shmem->reindeersHitched == NR){
        sem_post(&(shmem->allHitched));
    }

    exit(0);
}

/// MAIN
int main(int argc, char *argv[])
{
/**Storing values from standard input. All parametres need to be given as whole numbers in their respective ranges, 
 * else the program prints the help message to standard error output and exits. */
    argsCheck(argc, argv);
    argsLoad(argv);
// Values read successfully, open file 'proj2.out' for writing
    fp = fopen("proj2.out", "w");
    //fprintf(fp,"%d\n",n); -- this is how to write into the file
// Use current time as seed for random number generator
    srand(time(0));
// Initialize shared memory
    int id;

    id = shmget(shmem_key, 20 * sizeof(sharedMem), IPC_CREAT | 0644);
    shmem = (sharedMem *) shmat(id, NULL, 0);
// Initialize semaphores
    initSemaphores(shmem, id);
// Create child processes for Santa, Elves and Reindeers

    int idS = fork();
    if (idS == -1) {
        fprintf(stderr,"Fork error for Santa\n");
        freeShmem(shmem,id);
        exit (1);
    } else if (idS == 0) {
        santaFunc(shmem);
    }
    
    int more;
    if (NE > NR){
        more = NE;
    } else {
        more = NR;
    }
    
    for (int i = 1, j = 1, l = 1; l <= more+1; i++, j++, l++) {
        if (i < NE+1){
            int elfID = fork();
            if (elfID == -1) {
                fprintf(stderr,"Fork error for elf #%d\n", i);
                freeShmem(shmem,id);
                exit (1);
            } else if (elfID == 0) { //if we're a child process
                elfFunc(i, shmem);
            }
        }
        if (j < NR+1){
            int rdID = fork();
            if (rdID == -1){
                fprintf(stderr,"Fork error for reindeer #%d\n", j);
                freeShmem(shmem,id);
                exit (1);
            } else if (rdID == 0) { //if we're a child process
                reindeerFunc(j, shmem);
            }
        }
    }

// Wait for all child processes to die
    while (wait(NULL) != -1 || errno != ECHILD);
// Release and clear the shared memory
    freeShmem(shmem,id);

//auxiliary print just for checking args (comment out in finished project)
    //printf("\nNE = %d\nNR = %d\nTE = %d\nTR = %d\n",NE,NR,TE,TR);
    
    return 0;
}