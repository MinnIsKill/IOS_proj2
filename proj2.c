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
    int action;         // action counter
    int reindeersReady; // ready reindeers

	sem_t elf; 	// used for santa to wait on the elf speech
	sem_t reindeer;  	// used by santa to weight on satisfaction from each elf
	sem_t santa; // protects elf counter
} sharedMem;


/// GLOBAL VARIABLES
int NE = 0;
int NR = 0;
int TE = 0;
int TR = 0;

FILE *fp;

/// FUNCTIONS

void argsCheck(int argc, char* argv[]){
    long val;
    char *next;
	if(argc!=5){ //test for number of arguments and floats/non-number characters
        printf("%s",helpmsg);
        exit(1);
    }
    for (int i = 1; i < argc; i++) { //process all arguments one-by-one
        val = strtol (argv[i], &next, 10); //get value of arguments, stopping when NoN encountered
        if ((val%1 != 0) || (next == argv[i]) || (*next != '\0')) { // Check for empty string and characters left after conversion
            printf("%s",helpmsg);
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
		printf("%s",helpmsg); /// print help
		exit(1);
	}
}

void santaFunc(sharedMem* s){
    printf("%-3d   I'm Santa\n",s->action++);
    while(1){
        if (s->reindeersReady == NR){
            printf("%-3d   Hohoho, Merry Christmas!\n",s->action++);
            exit(0);
        }
    }
}

void elfFunc(int num, sharedMem *s){
    printf("%-3d   I'm elf #%d\n",s->action++, num);
    usleep((random() % (TE + 1)) * 1000);
    printf("%-3d   Elf #%d did something\n",s->action++, num);
    exit(0);
}

void reindeerFunc(int num, sharedMem* s){
    printf("%-3d   I'm reindeer #%d\n",s->action++, num);
    usleep((random() % (TR + 1)) * 1000);
    printf("%-3d   Reindeer #%d ready\n",s->action++, num);
    s->reindeersReady++;
    exit(0);
}

/// MAIN
int main(int argc, char *argv[])
{
/**
 * Storing values from standard input. All parametres need to be given as whole numbers in their respective ranges, 
 * else the program prints the help message to standard error output and exits.
 */
    argsCheck(argc, argv);
    argsLoad(argv);
// Values read successfully, open file 'proj2.out' for writing
    fp = fopen("proj2.out", "w");
    //fprintf(fp,"%d\n",n); -- this is how to write into the file

    int id;
    sharedMem *shmem;

    id = shmget(shmem_key, 20 * sizeof(sharedMem), IPC_CREAT | 0644);
    shmem = (sharedMem *) shmat(id, NULL, 0);


    int ids = fork();
    if (ids == -1) {
        printf("Fork error for Santa");
    } else if (ids == 0) {
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
            int ide = fork();
            if (ide == -1) {
                printf("Fork error (elf #%d)", i);
                exit (1);
            } else if (ide == 0) {
                elfFunc(i, shmem);
            }
        }
        if (j < NR+1){
            int idr = fork();
            if (idr == -1){
                printf("Fork error (reindeer #%d)", j);
                exit (1);
            } else if (idr == 0) {
                reindeerFunc(j, shmem);
            }
        }
    }
    /**if (fork() == -1) {

    } else if ()**/

    shmdt(shmem);
    shmctl(id, IPC_RMID, NULL);

    while (wait(NULL) != -1 || errno != ECHILD); // wait for all child processes to finish

    //auxiliary print just for checking
    printf("\nNE = %d\nNR = %d\nTE = %d\nTR = %d\n",NE,NR,TE,TR);
    
    return 0;
}