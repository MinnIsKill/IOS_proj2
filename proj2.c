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
#include <semaphore.h>


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






/// MAIN
int main(int argc, char *argv[])
{
/**
 * Storing values from standard input. All parametres need to be given as whole numbers in their respective ranges, 
 * else the program prints the help message to standard error output and exits.
 */
    argsCheck(argc, argv);
    argsLoad(argv);
/**
 * Values read successfully, open file 'proj2.out' for writing
 */
	fp = fopen("proj2.out", "w"); /// open file 'proj2.out' for writing
    //fprintf(fp,"%d\n",n); -- this is how to write to the file

    //auxiliary print just for checking
    printf("NE = %d\nNR = %d\nTE = %d\nTR = %d\n",NE,NR,TE,TR);
    return 0;
}