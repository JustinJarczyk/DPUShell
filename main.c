#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


//////////////////////////////////////////////////////////////////
// SIGNAL HANDLERS/ INTERRUPTS (create a method to handle signals)
//////////////////////////////////////////////////////////////////
void signal_handler(int signal_number){
    // catch <CTRL-C>
    if (signal_number == SIGINT){
        printf("received SIGINT\n");
    }
}

//////////////////////////////////////////////////////////////////
// JOBS/DATA STRUCTURES (holds all information about jobs, getters, setters, list accessibility)
//////////////////////////////////////////////////////////////////
// holds all info for a shell job
// struct reference: https://stackoverflow.com/questions/10162152/how-to-work-with-string-fields-in-a-c-struct
typedef struct job
{
    int id;
    int pid;
    int state;
    char *command;
} job;

// because chars can be of variable size
// we need to allocate them based on the command
// we don't want to over allocate memory -> inefficient
job *createJob(int pid, int state, char *command){

    job *j = malloc(sizeof(struct job));
    j->id = -1; // initialize with -1 until added to the jobslist
    j->pid = pid;
    j->state = state;
    j->command = strdup(command);

    return j;
}

// create a job list
// chose a linked list for easy traversal
typedef struct jobsllist
{
    job *job;
    struct jobsllist *next;

} jobsllist;

// Jobs List helper methods
job *getJobFromJobsListById(jobsllist *jobs, int id){
    jobsllist *l = jobs;
    while (l != NULL) {
        if (l->job->id == id) {
            return l->job;
        }
        l = l->next;
    }
    return NULL;
}

void *listJobsListJobs(jobsllist *jobs){
    jobsllist *l = jobs;
    while (l != NULL) {
        printf("[%i]\t[%i]\t[%i]\t[%s]\n"
                , l->job->id
                , l->job->pid
                , l->job->state
                , l->job->command
        );
        l = l->next;
    }
    return 0;
}

// adds job to the jobslist
void *addJobsListJob(jobsllist *jobslist, job *j){

    // initialize first element, if not already
    if (jobslist->job == NULL){
        j->id = 0;
        jobslist->job = j;
        jobslist->next = NULL;

    } else {
        int jobid = 1;
        jobsllist *l = jobslist;

        while (l->next != NULL) {
            jobid++; //increment the jobid
            l = l->next;
        }

        j->id = jobid;
        l->next = malloc(sizeof(struct jobsllist));
        l->next->job = j;
        l->next->next = NULL;
    }
    return 0;
}

//////////////////////////////////////////////////////////////////
// COMMAND STRUCTURES (holds all information about commands and builtin commands
//////////////////////////////////////////////////////////////////
/*
// struct to process command chains
typedef struct shellcommand
{
    // individual command
    char *command;
    char *arguements;

    //redirection (pipes & carrots)
    int isPipeToNextCommand;
    int isWriteToFile;
    int isAppendToFile;

    int isBackground;
    struct shellcommand *next;
} shellcommand;


shellcommand *processCommand(char *command){

    //shellcommand *c = malloc(sizeof(struct command));

    // is background, if it is remove element

    // evaluates carrots from right to left, in order of operations





    //return c;
}
*/



int isBuiltinShellCommand(jobsllist *jobslist, char *command){

    int retVal = 0;

    // built in shell command, list jobs
    if (strcmp(command,"jobs") == 0){
        listJobsListJobs(jobslist);
        retVal = 1;
    }

    return retVal;
}


// main runtime constants
const int RUNNING_FOREGROUND = 1;
const int RUNNING_BACKGROUND = 2;
const int STOPPED_BACKGROUND = 3;


// main runtime statics
static char command[1024];
static jobsllist shelljobs[0];

int main() {

    // add the shell to the jobs list
    addJobsListJob(shelljobs, createJob(getpid(), RUNNING_FOREGROUND ,"/bin/DPUShell"));

    //printf("Hello, World testing now!\n");
    //printf("My process ID : %d\n", getpid());
    //printf("My parent's ID: %d\n", getppid());


    // register the signal handlers to the above signal_handler function
    if (signal(SIGINT, signal_handler) == SIG_ERR) // Ctrl+C
        printf("\ncan't catch SIGINT\n");

    // start the event shell
    while(1){
        printf("&> ");

        // get user input
        if (!fgets(command, 1024, stdin)) return 0;


        // clean input and verify command is not nothing
        command[strlen(command) - 1] = 0; // replace \n w/ \0


        if (strlen(command) == 0) continue;


        // check for builtin shell commands
        if (!isBuiltinShellCommand(shelljobs, command)){


            // creates new job, for the action
            job *newjob = createJob(0, RUNNING_FOREGROUND , command); // TODO:// Insert the pid and the running state of the job
            addJobsListJob(shelljobs, newjob);

        }

        //printf("Executing command: [%s] strlen [%i]\n", command, strlen(command));

    }

    return 0;
}