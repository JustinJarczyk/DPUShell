#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <MacTypes.h>


const int MAX_READ_SIZE = 1024;
const int MAX_WRITE_SIZE = 1024;

// TODO:// can have these dynamically allocate instead of hardcoded values
const int MAX_COMMANDS_IN_ONE_LINER = 1024; // amount of total commands that can be processed in a one liner input command

//////////////////////////////////////////////////////////////////
// Helpers, to meet project kernel requirements
//////////////////////////////////////////////////////////////////

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
const int DOUBLE_GREATER_THAN_SYMBOL = 3;

// holds the context in which the commands will be processed
// errors, symbols, etc... as well as commands.
typedef struct shellcontext
{
    int greater_than_count;
    int less_than_count;
    int triple_or_more_greater_than_symbol_errors;
    struct shellcommand *shellcommand;

} shellcontext;

// struct to process command chains
typedef struct shellcommand
{
    // individual command
    char *command;
    char *base_command;
    char *arguments;
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

int isValueInArray(int val, int *arr, int size){
    int i;
    for (i=0; i < size; i++) {
        if (arr[i] == val)
            return 1;
    }
    return 0;
}

shellcontext* processCommand(char* input){

    char cmd[strlen(input)+1];
    strncpy(cmd, input, sizeof(cmd));

    shellcommand *c = (struct shellcommand *)malloc(sizeof(struct shellcommand));

    // trim command trailing/leading characters
    char command_to_clean[strlen(cmd)];

    int titor = 0;
    int trim_leading = 1;

    for(int u=0; u<strlen(cmd); u++){
        if (trim_leading && (cmd[u] == ' ')) continue;
        command_to_clean[titor] = cmd[u];
        titor++;
        trim_leading = 0;
    }

    // remove trailing whitespace
    int w;
    for(w=titor-1; w > 0; w--){
        if(command_to_clean[w] == ' '){
            titor--;
        } else {
            break;
        }
    }

    char cleaned_command[titor];
    for(int q=0; q<titor; q++){
        cleaned_command[q] = command_to_clean[q];
    }

    cleaned_command[titor] = '\0';
    char* command = malloc(strlen(cleaned_command) + 1);
    strcpy(command, cleaned_command);


    // begin processing arrays and positions
    int greater_than_count = 0;
    int less_than_count = 0;

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

    // get the position within the array for all less than and greater than
    int gposcount = 0;
    int lposcount = 0;

    int greater_than_positions[greater_than_count+1];
    int less_than_positions[less_than_count+1];
    greater_than_positions[0] = -1;
    less_than_positions[0] = -1;

    int k;
    for(k=0; command[k]!= '\0';k++){
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

        char placeholder[2];
        placeholder[0] = '\0';
        tmpshcmd->command = malloc( strlen(placeholder) + 1);
        strcpy(tmpshcmd->command, placeholder);


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

            int search_for_leading_whitespace = 1;
            int m;
            for (m=idx; m <= next-1; m++){
                // bypass leading whitespace
                if ((search_for_leading_whitespace) && (command[m] == ' ')) continue;
                tmp[tmpitor] = command[m];
                tmpitor++;
                search_for_leading_whitespace = 0;
            }

            // remove trailing whitespace
            int n;
            for(n=tmpitor-1; n > 0; n--){
                if(tmp[n] == ' '){
                    tmpitor--;
                } else {
                    break;
                }
            }
            // create a new array without trailing characters
            char rmtrailing[tmpitor];
            for(int g=0; g<tmpitor; g++){
                rmtrailing[g] = tmp[g];
            }

            if (tmp[0] != '\0'){
                rmtrailing[tmpitor] = '\0';

                tmpshcmd->command = malloc(strlen(rmtrailing) + 1);
                strcpy(tmpshcmd->command, rmtrailing);

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
        }  else {
            // handle the double symbol case [>>]
            if (command[idx-1] == '>' && command[next] == '>') {
                tmpshcmd->command = malloc(2);
                char carrots[3];
                carrots[0] = command[idx-1];
                carrots[1] = command[next];
                carrots[2] = '\0';

                strcpy(tmpshcmd->command, carrots);
                tmpshcmd->proceeding_special_character = DOUBLE_GREATER_THAN_SYMBOL;
                tmpshcmd->next = (struct shellcommand *)malloc(sizeof(struct shellcommand));
                tmpshcmd = tmpshcmd->next;
                tmpshcmd->next = NULL;

            }
        }


        idx = next+1;
    }

    int triple_or_more_greater_than_symbol_errors = 0;
    // clean up the linked list of commands
    shellcommand* ll = c;
    while (ll->next != NULL) {
        // clean up >> & >>> duplicate entries
        if ((ll->proceeding_special_character == GREATER_THAN_SYMBOL) &&
                (ll->next->proceeding_special_character == DOUBLE_GREATER_THAN_SYMBOL)){
            // we have a potential case of >> or >>> n+1
            // we need to verify that the next of the next symbol is not mis interpreted as >>
            // this is an error case that needs to be accounted for
            if ((ll->next->next != NULL) &&
                    (ll->next->next->proceeding_special_character == DOUBLE_GREATER_THAN_SYMBOL)){
                // we have hit the edge case of >>> or >> + n*(>) example: >>>>>>>>>>>
                shellcommand* ltmp = ll->next;
                while(ltmp->next != NULL
                      && ltmp->next->proceeding_special_character == DOUBLE_GREATER_THAN_SYMBOL){
                    shellcommand* lltmp = ltmp;
                    ltmp = ltmp->next;
                    free(lltmp);
                }
                // we've by passed all of the double characters links now to clean up to indicate the error
                ll->proceeding_special_character = DOUBLE_GREATER_THAN_SYMBOL;
                ltmp->command[1]= '\0'; // remove the >> make >
                ltmp->proceeding_special_character = GREATER_THAN_SYMBOL;
                triple_or_more_greater_than_symbol_errors = 1;

                if(ltmp->next != NULL) {
                    ll->next = ltmp;
                } else {
                    ll->next = NULL;
                }

            } else {
                // we have hit the >> case and need to clean up the mis-interpretation
                shellcommand* ltmp = ll->next;
                ll->proceeding_special_character = DOUBLE_GREATER_THAN_SYMBOL;
                // skip the >> link and go to the next command
                if (ll->next->next!=NULL) {
                    ll->next = ltmp->next;
                    free(ltmp);
                } else {
                    ll->next = NULL;
                }
                // keep the correct count for the input redirects
                greater_than_count = greater_than_count-2;
            }
        }

        ll = ll->next;
    }

    shellcommand* lll = c;
    // get the arguments from the context
    while (lll->next != NULL) {
        if (lll->command != NULL && lll->command[0] != '\0'){

            int bitor = 0;
            int aitor = 0;

            char base_command[strlen(lll->command)];
            char arguments[strlen(lll->command)];

            int done_with_base = 0;

            for(int i=0;i < strlen(lll->command);i++){

                if (!done_with_base){
                    // process the base command
                    if ((lll->command[i] != '\0') && lll->command[i] != ' '){
                        base_command[bitor] = lll->command[i];
                        bitor++;
                    }
                    if (lll->command[i] == ' '){
                        done_with_base = 1;
                    }
                } else {
                    // process the arguments
                    if ((lll->command[i] != '\0')){
                        arguments[aitor] = lll->command[i];
                        aitor++;
                    }
                }
            }

            base_command[bitor] = '\0';
            arguments[aitor] = '\0';

            c->base_command = malloc(bitor + 1);
            strcpy(c->base_command, base_command);

            c->arguments = malloc(aitor + 1);
            strcpy(c->arguments, arguments);
        }
        lll = lll->next;
    }

    shellcontext *sc = (struct shellcontext *)malloc(sizeof(struct shellcontext));
    sc->greater_than_count = greater_than_count;
    sc->less_than_count = less_than_count;
    sc->triple_or_more_greater_than_symbol_errors = triple_or_more_greater_than_symbol_errors;
    sc->shellcommand = c;

    return sc;

}


int isBuiltinShellCommand(jobsllist *jobslist, shellcontext *shcntx){

    int retVal = 0;

    //printf("base_command = [%s]\n", shcntx->shellcommand->base_command);
    //printf("arguements = [%s]\n", shcntx->shellcommand->arguments);



    // built in shell command, list jobs
    if (strcmp(shcntx->shellcommand->base_command,"jobs") == 0){
        listJobsListJobs(jobslist);
        retVal = 1;
    }
    if (strcmp(shcntx->shellcommand->base_command,"cd") == 0){
        // check to make sure directory is specified within the arguements
        if (shcntx->shellcommand->arguments != NULL || shcntx->shellcommand->arguments != '\0'){
            chdir(shcntx->shellcommand->arguments);
        } else {
            printf("ERROR - Can’t cd without a file path");
        }
        retVal = 1;
    }
    if (strcmp(shcntx->shellcommand->base_command,"ln") == 0){


        if (shcntx->shellcommand->arguments != NULL || shcntx->shellcommand->arguments != '\0'){

            // need to split the source and destination into 2 from the arguements.


        } else {
            printf("ERROR - Can't link without source/destination");
        }
        // use the link command


        retVal = 1;
    }

    /*
    if (strcmp(shcntx->shellcommand->base_command,"jobs") == 0){

        retVal = 1;
    }
    */

    return retVal;
}

const int TOO_MANY_INPUT_REDIRECTS_IN_1_LINE = 1000;
const int NO_REDIRECTION_FILE_SPECIFIED = 2000;
const int TRIPLE_OR_MORE_GREATER_THAN_SYMBOLS = 3000;
const int NO_COMMAND = 4000;
//  1) no 2 input redirects in 1 line
//  2) redirection file must be specified

int shellCommandErrorsExist(shellcontext *shellcontext){
    int response = 0;

    // check if >>> or more exist in a line
    if(shellcontext->triple_or_more_greater_than_symbol_errors){
        response = TRIPLE_OR_MORE_GREATER_THAN_SYMBOLS;
    }

    // check for too many redirects
    if((response == 0) && (shellcontext->less_than_count > 1)){

        response = TOO_MANY_INPUT_REDIRECTS_IN_1_LINE;
    }



    // check for valid input file
    shellcommand * l = shellcontext->shellcommand;
    while ((response == 0) && (l->next != NULL)) {
        // check for redirect
        if ((l->proceeding_special_character == LESS_THAN_SYMBOL) &&
                (l->next != NULL)){

            // NEED TO HANDLE NULL LOGIC DOWN HERE

            // check for valid input not ""
            if (l->next->command == '\0'){
                response = NO_REDIRECTION_FILE_SPECIFIED;
            } else {
                // check if file exists on disk to redirect from
                // case :  [cat < file.txt]
                // TODO://
            }


        }

        l = l->next;
    }

    // check for valid command
    shellcommand * ll = shellcontext->shellcommand;
    while ((response == 0) && (ll->next != NULL)) {

        // Check fo rthe correct
        if ((ll->proceeding_special_character == GREATER_THAN_SYMBOL) &&
                (ll->command[0] == '\0')){
            // ERROR ERROR - No command.
            response = NO_COMMAND;
        }
        ll = ll->next;
    }

    return response;
}


// main runtime constants
const int RUNNING_FOREGROUND = 1;
const int RUNNING_BACKGROUND = 2;
const int STOPPED_BACKGROUND = 3;


static jobsllist shelljobs[0];

void debug_environment(char **envp);

int main(int argc, char **argv, char **envp) {

    char command[MAX_READ_SIZE];

    //debug_environment(envp);

    // add the shell to the jobs list
    addJobsListJob(shelljobs, createJob(getpid(), RUNNING_FOREGROUND ,"/bin/DPUShell"));

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

        if (strlen(command) == 0) continue;

        // get the shell context and process command for execution
        shellcontext* shcntx = processCommand(command);

        listShellCommands(shcntx->shellcommand);

        int errors_exist = 0;

        switch (shellCommandErrorsExist(shcntx)){

            case TOO_MANY_INPUT_REDIRECTS_IN_1_LINE:
                printf("ERROR - Can’t have two input redirects on one line\n");
                errors_exist = 1;
                break;
            case NO_REDIRECTION_FILE_SPECIFIED:
                printf("ERROR - No redirection file specified\n");
                errors_exist = 1;
                break;
            case TRIPLE_OR_MORE_GREATER_THAN_SYMBOLS:
                printf("ERROR - Cannot have >>> or more [>]\n");
                errors_exist = 1;
                break;
            case NO_COMMAND:
                printf("ERROR - No command\n");
                errors_exist = 1;
                break;
        }

        // check for builtin shell commands
        if (!isBuiltinShellCommand(shelljobs, shcntx) && !errors_exist){

            //




            //printf("greater than count = [%i]\n", shcntx->greater_than_count);
            //printf("less than count = [%i]\n", shcntx->less_than_count);
            //listShellCommands(shcntx->shellcommand);





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
                    printf("sh: command not found: [%s]\n", argv[0]);
                    exit(1);
                } else {
                    printf("sh: execution of %s failed: [%s]\n", argv[0], strerror(errno));
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




