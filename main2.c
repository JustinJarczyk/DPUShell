#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>



const int MAX_READ_SIZE = 1024;
const int MAX_WRITE_SIZE = 1024;

// TODO:// can have these dynamically allocate instead of hardcoded values
const int MAX_COMMANDS_IN_ONE_LINER = 1024; // amount of total commands that can be processed in a one liner input command

//////////////////////////////////////////////////////////////////
// Helpers, to meet project kernel requirements
//////////////////////////////////////////////////////////////////
/*
// use the kernel apis, don't want to sacrifice string formatting
void _dpu_log(int fd, char *fmt, va_list arguments){

    char outtxt[MAX_WRITE_SIZE];
    if (!vsprintf(outtxt, fmt, arguments)) {
        raise(0); //TODO:// add in an error message
    }

    write(fd, outtxt, strlen(outtxt));
}


void dpulog(char *fmt, ...){

    va_list arguments;
    va_start(arguments, fmt);
    _dpu_log(STDOUT_FILENO, fmt, arguments);
    va_end(arguments);

}

void dpulogerr(char *fmt, ...){

    va_list arguments;
    va_start(arguments, fmt);
    _dpu_log(STDERR_FILENO, fmt, arguments);
    va_end(arguments);

}
*/

int dpuread(char* command){
    int num_read = read(STDIN_FILENO, command, MAX_READ_SIZE);
    return num_read;

}

//////////////////////////////////////////////////////////////////
// SIGNAL HANDLERS/ INTERRUPTS (create a method to handle signals)
//////////////////////////////////////////////////////////////////
void signal_handler(int signal_number){
    // catch <CTRL-C>
    if (signal_number == SIGINT){
        //dpulogerr("received SIGINT\n");
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

    job *j = (struct job *)malloc(sizeof(struct job));
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
        l->next = (struct jobsllist *)malloc(sizeof(struct jobsllist));
        l->next->job = j;
        l->next->next = NULL;
    }
    return 0;
}

//////////////////////////////////////////////////////////////////
// COMMAND STRUCTURES (holds all information about commands and builtin commands
//////////////////////////////////////////////////////////////////
const int NO_CHARACTER = 0;
const int GREATER_THAN_SYMBOL = 1;
const int LESS_THAN_SYMBOL = 2;


// struct to process command chains
typedef struct shellcommand
{
    // individual command
    char *command;
    int proceeding_special_character;
    struct shellcommand *next;
} shellcommand;


void *listShellCommands(shellcommand *commands){
    shellcommand *l = commands;
    while (l->next != NULL) {
        printf("[%s]\n[%i]\n", l->command, l->proceeding_special_character);
        l = l->next;
    }
    return 0;
}

/*

int str2tokens2Array(char* input, char* cmd_arr[], char* delimeter){

    // tokenize the input
    char* token_pointer = strtok(input, delimeter);
    int i = 0;
    while (token_pointer != NULL) {
        cmd_arr[i] = token_pointer;
        token_pointer = strtok(NULL, delimeter);
        i++;
    }
    cmd_arr[i] = NULL; // set last element to null.

    return i-1;
}
 */


int isValueInArray(int val, int *arr, int size){
    int i;
    for (i=0; i < size; i++) {
        if (arr[i] == val)
            return 1;
    }
    return 0;
}

shellcommand* processCommand(char* command){

    // is background, if it is remove element
    // evaluates carrots from right to left, in order of operations
    //return c;

    shellcommand *c = (struct shellcommand *)malloc(sizeof(struct shellcommand));

    int greater_than_count = 0;
    int less_than_count = 0;

    printf("command [%s]\n", command);

    // get the counts to initialize the position arrays
    int z = 0;
    for(z=0; command[z]!= '\0';z++){
        //printf("char command[%i] = %c\n",z, command[z]);
        if (command[z] == '>'){
            greater_than_count++;
        }
        if (command[z] == '<'){
            less_than_count++;
        }
    }

    printf("greater than count = [%i] \n", greater_than_count);
    printf("less than count = [%i] \n", less_than_count);

    // get the position within the array for all less than and greater than
    int gposcount = 0;
    int lposcount = 0;

    int greater_than_positions[greater_than_count+1];
    int less_than_positions[less_than_count+1];
    greater_than_positions[0] = -1;
    less_than_positions[0] = -1;


    int k;
    for(k=0; command[k]!= '\0';k++){
        //printf("char command[%i] = %c\n",z, command[z]);
        if (command[k] == '>'){
            greater_than_positions[gposcount] = k;
            gposcount++;
        }
        if (command[k] == '<'){
            less_than_positions[lposcount] = k;
            lposcount++;
        }
    }

    // set the element after the end to an unreasonably high number as to not
    // interfere with the below comparison
    less_than_positions[lposcount] = 10000000;
    greater_than_positions[gposcount] = 10000000;

    // combine both integer arrays and sort
    int special_char_positions[greater_than_count + less_than_count + 1];

    int x;
    int gpos = 0;
    int lpos = 0;

    for (x = 0; x < (greater_than_count+ less_than_count); x++){

        if ((less_than_count > 0) && (greater_than_count > 0)) {

            if (greater_than_positions[gpos] < less_than_positions[lpos]) {
                special_char_positions[x] = greater_than_positions[gpos];
                gpos++;
            } else {
                special_char_positions[x] = less_than_positions[lpos];
                lpos++;
            }
        } else if (greater_than_count > 0){
            special_char_positions[x] = greater_than_positions[gpos];
            gpos++;
        } else if  (less_than_count > 0){
            special_char_positions[x] = less_than_positions[lpos];
            lpos++;
        }
    }

    shellcommand* tmpshcmd = c;

    //initialize first shell command if > or < is in the first position of command
    if ((command[0] == '<')||(command[0] == '>')){

        tmpshcmd->command = malloc(strlen("") + 1);
        strcpy(tmpshcmd->command, "");

        // determine the proceeding symbol
        if( isValueInArray(0, greater_than_positions, greater_than_count) ){
            tmpshcmd->proceeding_special_character = GREATER_THAN_SYMBOL;
        } else if( isValueInArray(0, less_than_positions, less_than_count)){
            tmpshcmd->proceeding_special_character = LESS_THAN_SYMBOL;
        } else {
            tmpshcmd->proceeding_special_character = NO_CHARACTER;
        }
        tmpshcmd->next = (struct shellcommand *)malloc(sizeof(struct shellcommand));
        tmpshcmd = tmpshcmd->next;
        tmpshcmd->next = NULL;
    }

    int idx = 0; //hold the index bounds
    int next;
    int l;
    for(l=0;l <= (greater_than_count+ less_than_count); l++){
        // iterate over each position
        if (l < (greater_than_count+ less_than_count)){
            next = special_char_positions[l];
        } else {
            next = strlen(command) + 1;
        }

        int chars_in_between_indexes = ((next-1)-idx);
        // check if anything exists between special characters
        if (chars_in_between_indexes >= 0){

            char tmp[chars_in_between_indexes + 1];
            int tmpitor = 0;

            int m;
            for (m=idx; m <= next-1; m++){
                tmp[tmpitor] = command[m];
                tmpitor++;
            }

            if (tmp[0] != '\0'){
                tmp[tmpitor] = '\0';

                tmpshcmd->command = malloc(strlen(tmp) + 1);
                strcpy(tmpshcmd->command, tmp);

                // determine the proceeding symbol
                if( isValueInArray(m, greater_than_positions, greater_than_count) ){
                    tmpshcmd->proceeding_special_character = GREATER_THAN_SYMBOL;
                } else if( isValueInArray(next, less_than_positions, less_than_count)){
                    tmpshcmd->proceeding_special_character = LESS_THAN_SYMBOL;
                } else {
                    tmpshcmd->proceeding_special_character = NO_CHARACTER;
                }
                tmpshcmd->next = (struct shellcommand *)malloc(sizeof(struct shellcommand));
                tmpshcmd = tmpshcmd->next;
                tmpshcmd->next=NULL;
            }
        } else {

            tmpshcmd->command = malloc(2);
            strcpy(tmpshcmd->command, "");
            tmpshcmd->proceeding_special_character = NO_CHARACTER;
            tmpshcmd->next = NULL;
        }
        idx = next+1;
    }


    /// THERE IS A DAMN MEMORY LEAK EITHER IN LIST SHELL COMMANDS OR HOW I AM CREATING THE SHELL COMMANDS

    listShellCommands(c);







    //char subbuff[5];
    //memcpy( subbuff, &buff[10], 4 );
    //subbuff[4] = '\0';



    /*
    printf("command = [%s]\n", command);
    ////////////////////////////////////////////////////
    // get less than carrots
    ////////////////////////////////////////////////////
    // Rules
    //  1) no 2 input redirects in 1 line
    //  2) redirection file must be specified
    char * less_than_carrot_array[MAX_COMMANDS_IN_ONE_LINER];
    int amount_less_than_carrots = str2tokens2Array(command, less_than_carrot_array, "<");
    printf("number of less than carrots: %i \n", amount_less_than_carrots);
*/
    /*
    int k;
    for(k=0;less_than_carrot_array[k] != NULL;k++) {
        printf("less_than_carrot_array[%i] = %s \n", k, less_than_carrot_array[k]);
    }
    int amount_less_than_carrots = k-1; // if greater than 2 error
    printf("number of less than carrots: %i \n", amount_less_than_carrots);
    */
    ////////////////////////////////////////////////////
    // get greater than carrots
    ////////////////////////////////////////////////////
    // Rules
    //  1)
    //  2)
/*
    printf("command = [%s]\n", command);

    char * greater_than_carrot_array[MAX_COMMANDS_IN_ONE_LINER];
    int amount_greater_than_carrots = str2tokens2Array(command, greater_than_carrot_array, ">");
    printf("number of greater than carrots: %i \n", amount_greater_than_carrots);


    int j;
    for(j=0;greater_than_carrot_array[j] != NULL;j++) {
        printf("greater_than_carrot_array[%i] = %s \n", j, greater_than_carrot_array[j]);
    }
    //int amount_greater_than_carrots = j-1; // if greater than 2 error
    //printf("number of greater than carrots: %i \n", amount_greater_than_carrots);




*/


    // individual command
    //c->command = strdup(command);
    /*
    char* token_pointer = strtok(c->command, " ");
    char* token_array[MAX_TOKENS_CMDLINE_ARGS];
    int i = 0;
    while (token_pointer != NULL) {
        // store the tokens in the array
        token_array[i] = token_pointer;
        printf("token[%i]: %s \n", i, token_pointer);
        token_pointer = strtok(NULL, " ");
    }
    */







    //int i;
    //for(i = 0; c->arguments[i] != '\0'; i++)
    //{
        //printf("%c\n", c->arguments[i]);
    //}

    return c;
    //c->arguements = strdup(strtok(command, ">"));

    //redirection (pipes & carrots)

    //int isPipeToNextCommand;
    //int isWriteToFile;
    //int isAppendToFile;

    //int isBackground;
    //struct shellcommand *next;

    /*



    j->id = -1; // initialize with -1 until added to the jobslist
    j->pid = pid;
    j->state = state;
    j->command = strdup(command);
*/
}


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

static jobsllist shelljobs[0];



void debug_environment(char **envp);

int main(int argc, char **argv, char **envp) {

    char command[MAX_READ_SIZE];

    //debug_environment(envp);

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
        fflush( stdout );

        // get user input
        if (!dpuread(command)){
            return 0;
        }


        // clean input and verify command is not nothing
        command[strlen(command) - 1] = 0; // replace \n w/ \0


        if (strlen(command) == 0){
            continue;
        }


        // check for builtin shell commands
        if (!isBuiltinShellCommand(shelljobs, command)){

            shellcommand* shcmd = processCommand(command);


            /*


            pid_t child_pid, wait_pid;
            int status;

            if (!(child_pid=fork())) { // child process executes this code here
                // now in child process
                char *argv[] = {"ls", NULL};
                char *envp[] = { NULL };

                execlp(argv[0], argv[0]); //?? why does this work?????
                //execve(argv[0], argv, envp);

                // we won't get here unless execve failed
                if (errno == ENOENT){
                    printf("sh: command not found: \n", argv[0]);
                    exit(1);
                } else {
                    printf("sh: execution of %s failed: \n", argv[0], strerror(errno));
                    exit(1);
                }
            } else {
                // parent process continues to run code

                if (child_pid < 0){
                    // error forking
                    printf("Error Forking\n");

                } else {

                    // creates new job, for the action
                    job *newjob = createJob(child_pid, RUNNING_FOREGROUND , command); // TODO:// Insert the pid and the running state of the job
                    addJobsListJob(shelljobs, newjob);

                    do {
                        wait_pid = waitpid(child_pid, &status, WUNTRACED);
                    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
                }

            }
            */
        }

        /// dirty buffer need to clean to stop extra garbage from cropping up
        for (int i = 0; i < MAX_READ_SIZE; i++)
            command[i] = 0;
    }

    return 0;
}

void debug_environment(char **envp) {
    char** env;
    for (env = envp; *env != 0; env++)
    {
        char* thisEnv = *env;
        printf("%s\n", thisEnv);
    }
}




