#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

#define PIPE_READ 0
#define PIPE_WRITE 1

// read / write max sizes
const int MAX_READ_SIZE = 1024;

// Job States
const int RUNNING_FOREGROUND = 1;
const int RUNNING_BACKGROUND = 2;
const int STOPPED_BACKGROUND = 3;



//////////////////////////////////////////////////////////////////
// Helpers
//////////////////////////////////////////////////////////////////

int dpuread(char *command) {
    command[0] = '\0';
    int num_read;
    while (command[0] == '\0') {
        num_read = read(STDIN_FILENO, command, MAX_READ_SIZE);
    }
    return num_read;

}

int isValueInArray(int val, int *arr, int size) {
    int i;
    for (i = 0; i < size; i++) {
        if (arr[i] == val)
            return 1;
    }
    return 0;
}

//////////////////////////////////////////////////////////////////
// JOBS/DATA STRUCTURES (holds all information about jobs, getters, setters, list accessibility)
//////////////////////////////////////////////////////////////////
typedef struct job {
    int id;
    int pid;
    int state;
    char *command;
    size_t process_stdout_read_address;
    int readpipe;
} job;

// create a job list
// chose a linked list for easy traversal
typedef struct jobsllist {
    job *job;
    struct jobsllist *next;

} jobsllist;

// because chars can be of variable size
// we need to allocate them based on the command
// we don't want to over allocate memory -> inefficient
job *createJob(int pid, char *output_address_pointer, int readpipe, int state, char *command) {

    job *j = (struct job *) malloc(sizeof(struct job));
    j->id = -1; // initialize with -1 until added to the jobslist
    j->pid = pid;
    j->state = state;
    j->command = strdup(command);
    j->process_stdout_read_address = (size_t) output_address_pointer;
    j->readpipe = readpipe;

    return j;
}

void *removeJobFromJobsListByPID(jobsllist *jobs, int pid) {
    jobsllist *l = jobs;
    jobsllist *prev = NULL;
    while (l != NULL) {
        if (l->job->pid == pid) {
            if (l->next != NULL) {
                jobsllist *tmp = l;
                prev->next = l->next;
                free(tmp);
            } else {
                if (prev != NULL) {
                    // isn't the base DPUShell Job
                    prev->next = NULL;
                    free(l->job);
                    free(l);
                }
            }
        }
        prev = l;
        l = l->next;
    }
    return 0;
}

void *listJobsListJobs(jobsllist *jobs) {
    jobsllist *l = jobs;
    while (l != NULL) {

        if (l->job->id != 0) { // bypass the DPUSHell Job

            if (l->job->state == 1)
                printf("[%i]\t[%i]\t[FOREGROUND]\t[%s]\n", l->job->id, l->job->pid, l->job->command);
            else if (l->job->state == 2)
                printf("[%i]\t[%i]\t[BACKGROUND]\t[%s]\n", l->job->id, l->job->pid, l->job->command);
            else
                printf("[%i]\t[%i]\t[STOPPED]\t[%s]\n", l->job->id, l->job->pid, l->job->command);
        }
        l = l->next;
    }
    return 0;
}

// adds job to the jobslist
void *addJobsListJob(jobsllist *jobslist, job *j) {

    // initialize first element, if not already
    if (jobslist->job == NULL) {
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
        l->next = (struct jobsllist *) malloc(sizeof(struct jobsllist));
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
typedef struct shellcontext {
    int greater_than_count;
    int less_than_count;
    int triple_or_more_greater_than_symbol_errors;
    struct shellcommand *shellcommand;

} shellcontext;

// struct to process command chains
typedef struct shellcommand {
    // individual command
    char *command;
    char *base_command;
    char *arguments;
    int proceeding_special_character;
    struct shellcommand *next;
} shellcommand;


// debug function
void *listShellCommands(shellcommand *commands) {
    shellcommand *l = commands;
    while (l->next != NULL) {
        printf("[%s]\n[%i]\n", l->command, l->proceeding_special_character);
        l = l->next;
    }
    return 0;
}

shellcontext *processCommand(char *input) {

    char cmd[strlen(input) + 1];
    strncpy(cmd, input, sizeof(cmd));

    shellcommand *c = (struct shellcommand *) malloc(sizeof(struct shellcommand));

    // trim command trailing/leading characters
    char command_to_clean[strlen(cmd)];

    int titor = 0;
    int trim_leading = 1;

    for (int u = 0; u < strlen(cmd); u++) {
        if (trim_leading && (cmd[u] == ' ')) continue;
        command_to_clean[titor] = cmd[u];
        titor++;
        trim_leading = 0;
    }

    // remove trailing whitespace
    int w;
    for (w = titor - 1; w > 0; w--) {
        if (command_to_clean[w] == ' ') {
            titor--;
        } else {
            break;
        }
    }

    char cleaned_command[titor];
    for (int q = 0; q < titor; q++) {
        cleaned_command[q] = command_to_clean[q];
    }

    cleaned_command[titor] = '\0';
    char *command = malloc(strlen(cleaned_command) + 1);
    strcpy(command, cleaned_command);


    // begin processing arrays and positions
    int greater_than_count = 0;
    int less_than_count = 0;

    // get the counts to initialize the position arrays
    int z = 0;
    for (z = 0; command[z] != '\0'; z++) {
        if (command[z] == '>') {
            greater_than_count++;
        }
        if (command[z] == '<') {
            less_than_count++;
        }
    }

    // get the position within the array for all less than and greater than
    int gposcount = 0;
    int lposcount = 0;

    int greater_than_positions[greater_than_count + 1];
    int less_than_positions[less_than_count + 1];
    greater_than_positions[0] = -1;
    less_than_positions[0] = -1;

    int k;
    for (k = 0; command[k] != '\0'; k++) {
        if (command[k] == '>') {
            greater_than_positions[gposcount] = k;
            gposcount++;
        }
        if (command[k] == '<') {
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

    for (x = 0; x < (greater_than_count + less_than_count); x++) {

        if ((less_than_count > 0) && (greater_than_count > 0)) {

            if (greater_than_positions[gpos] < less_than_positions[lpos]) {
                special_char_positions[x] = greater_than_positions[gpos];
                gpos++;
            } else {
                special_char_positions[x] = less_than_positions[lpos];
                lpos++;
            }
        } else if (greater_than_count > 0) {
            special_char_positions[x] = greater_than_positions[gpos];
            gpos++;
        } else if (less_than_count > 0) {
            special_char_positions[x] = less_than_positions[lpos];
            lpos++;
        }
    }

    shellcommand *tmpshcmd = c;

    //initialize first shell command if > or < is in the first position of command
    if ((command[0] == '<') || (command[0] == '>')) {

        char placeholder[2];
        placeholder[0] = '\0';
        tmpshcmd->command = malloc(strlen(placeholder) + 1);
        strcpy(tmpshcmd->command, placeholder);


        // determine the proceeding symbol
        if (isValueInArray(0, greater_than_positions, greater_than_count)) {
            tmpshcmd->proceeding_special_character = GREATER_THAN_SYMBOL;
        } else if (isValueInArray(0, less_than_positions, less_than_count)) {
            tmpshcmd->proceeding_special_character = LESS_THAN_SYMBOL;
        } else {
            tmpshcmd->proceeding_special_character = NO_CHARACTER;
        }
        tmpshcmd->next = (struct shellcommand *) malloc(sizeof(struct shellcommand));
        tmpshcmd = tmpshcmd->next;
        tmpshcmd->next = NULL;
    }

    int idx = 0; //hold the index bounds
    int next;
    int l;
    for (l = 0; l <= (greater_than_count + less_than_count); l++) {
        // iterate over each position
        if (l < (greater_than_count + less_than_count)) {
            next = special_char_positions[l];
        } else {
            next = strlen(command) + 1;
        }

        int chars_in_between_indexes = ((next - 1) - idx);
        // check if anything exists between special characters
        if (chars_in_between_indexes >= 0) {

            char tmp[chars_in_between_indexes + 1];
            int tmpitor = 0;

            int search_for_leading_whitespace = 1;
            int m;
            for (m = idx; m <= next - 1; m++) {
                // bypass leading whitespace
                if ((search_for_leading_whitespace) && (command[m] == ' ')) continue;
                tmp[tmpitor] = command[m];
                tmpitor++;
                search_for_leading_whitespace = 0;
            }

            // remove trailing whitespace
            int n;
            for (n = tmpitor - 1; n > 0; n--) {
                if (tmp[n] == ' ') {
                    tmpitor--;
                } else {
                    break;
                }
            }
            // create a new array without trailing characters
            char rmtrailing[tmpitor];
            for (int g = 0; g < tmpitor; g++) {
                rmtrailing[g] = tmp[g];
            }

            if (tmp[0] != '\0') {
                rmtrailing[tmpitor] = '\0';

                tmpshcmd->command = malloc(strlen(rmtrailing) + 1);
                strcpy(tmpshcmd->command, rmtrailing);

                // determine the proceeding symbol
                if (isValueInArray(m, greater_than_positions, greater_than_count)) {
                    tmpshcmd->proceeding_special_character = GREATER_THAN_SYMBOL;
                } else if (isValueInArray(next, less_than_positions, less_than_count)) {
                    tmpshcmd->proceeding_special_character = LESS_THAN_SYMBOL;
                } else {

                    tmpshcmd->proceeding_special_character = NO_CHARACTER;
                }
                tmpshcmd->next = (struct shellcommand *) malloc(sizeof(struct shellcommand));
                tmpshcmd = tmpshcmd->next;
                tmpshcmd->next = NULL;
            }
        } else {
            // handle the double symbol case [>>]
            if (command[idx - 1] == '>' && command[next] == '>') {
                tmpshcmd->command = malloc(2);
                char carrots[3];
                carrots[0] = command[idx - 1];
                carrots[1] = command[next];
                carrots[2] = '\0';

                strcpy(tmpshcmd->command, carrots);
                tmpshcmd->proceeding_special_character = DOUBLE_GREATER_THAN_SYMBOL;
                tmpshcmd->next = (struct shellcommand *) malloc(sizeof(struct shellcommand));
                tmpshcmd = tmpshcmd->next;
                tmpshcmd->next = NULL;

            }
        }


        idx = next + 1;
    }

    int triple_or_more_greater_than_symbol_errors = 0;
    // clean up the linked list of commands
    shellcommand *ll = c;
    while (ll->next != NULL) {
        // clean up >> & >>> duplicate entries
        if ((ll->proceeding_special_character == GREATER_THAN_SYMBOL) &&
            (ll->next->proceeding_special_character == DOUBLE_GREATER_THAN_SYMBOL)) {
            // we have a potential case of >> or >>> n+1
            // we need to verify that the next of the next symbol is not mis interpreted as >>
            // this is an error case that needs to be accounted for
            if ((ll->next->next != NULL) &&
                (ll->next->next->proceeding_special_character == DOUBLE_GREATER_THAN_SYMBOL)) {
                // we have hit the edge case of >>> or >> + n*(>) example: >>>>>>>>>>>
                shellcommand *ltmp = ll->next;
                while (ltmp->next != NULL
                       && ltmp->next->proceeding_special_character == DOUBLE_GREATER_THAN_SYMBOL) {
                    shellcommand *lltmp = ltmp;
                    ltmp = ltmp->next;
                    free(lltmp);
                }
                // we've by passed all of the double characters links now to clean up to indicate the error
                ll->proceeding_special_character = DOUBLE_GREATER_THAN_SYMBOL;
                ltmp->command[1] = '\0'; // remove the >> make >
                ltmp->proceeding_special_character = GREATER_THAN_SYMBOL;
                triple_or_more_greater_than_symbol_errors = 1;

                if (ltmp->next != NULL) {
                    ll->next = ltmp;
                } else {
                    ll->next = NULL;
                }

            } else {
                // we have hit the >> case and need to clean up the mis-interpretation
                shellcommand *ltmp = ll->next;
                ll->proceeding_special_character = DOUBLE_GREATER_THAN_SYMBOL;
                // skip the >> link and go to the next command
                if (ll->next->next != NULL) {
                    ll->next = ltmp->next;
                    free(ltmp);
                } else {
                    ll->next = NULL;
                }
                // keep the correct count for the input redirects
                greater_than_count = greater_than_count - 2;
            }
        }

        ll = ll->next;
    }

    shellcommand *lll = c;
    // get the arguments from the context
    while (lll->next != NULL) {
        if (lll->command != NULL && lll->command[0] != '\0') {

            int bitor = 0;
            int aitor = 0;

            char base_command[strlen(lll->command)];
            char arguments[strlen(lll->command)];

            int done_with_base = 0;

            for (int i = 0; i < strlen(lll->command); i++) {

                if (!done_with_base) {
                    // process the base command
                    if ((lll->command[i] != '\0') && lll->command[i] != ' ') {
                        base_command[bitor] = lll->command[i];
                        bitor++;
                    }
                    if (lll->command[i] == ' ') {
                        done_with_base = 1;
                    }
                } else {
                    // process the arguments
                    if ((lll->command[i] != '\0' && lll->command[i] != '\n')) {
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

    shellcontext *sc = (struct shellcontext *) malloc(sizeof(struct shellcontext));
    sc->greater_than_count = greater_than_count;
    sc->less_than_count = less_than_count;
    sc->triple_or_more_greater_than_symbol_errors = triple_or_more_greater_than_symbol_errors;
    sc->shellcommand = c;

    return sc;

}

//////////////////////////////////////////////////////////////////
//  Builtin commands & Error Validation
//////////////////////////////////////////////////////////////////

int isBuiltinShellCommand(jobsllist *jobslist, shellcontext *shcntx) {

    int retVal = 0;

    // built in shell command, list jobs
    if (strcmp(shcntx->shellcommand->base_command, "jobs") == 0) {
        listJobsListJobs(jobslist);
        retVal = 1;
    }
    if (strcmp(shcntx->shellcommand->base_command, "cd") == 0) {
        // check to make sure directory is specified within the arguements
        if (shcntx->shellcommand->arguments != NULL || shcntx->shellcommand->arguments != '\0') {

            int chdir_result;
            if (shcntx->shellcommand->arguments[0] == '~') {
                chdir_result = chdir(
                        getenv("HOME")); // TODO:// handle the case where ~/dir/dir, this only takes into account cd ~
            } else {
                chdir_result = chdir(shcntx->shellcommand->arguments);
            }

            if (chdir_result < 0) perror("cannot change directory");
        } else {
            printf("ERROR - Can’t cd without a file path\n");
        }
        retVal = 1;
    }
    if (strcmp(shcntx->shellcommand->base_command, "ln") == 0) {

        if (shcntx->shellcommand->arguments != NULL || shcntx->shellcommand->arguments != '\0') {

            // need to split the source and destination into 2 from the arguements.
            int sitor = 0;
            int ditor = 0;

            char source[strlen(shcntx->shellcommand->arguments)];
            char dest[strlen(shcntx->shellcommand->arguments)];

            int done_with_base = 0;

            for (int i = 0; i < strlen(shcntx->shellcommand->arguments); i++) {

                if (!done_with_base) {
                    // process the base command
                    if ((shcntx->shellcommand->arguments[i] != '\0') && shcntx->shellcommand->arguments[i] != ' ') {
                        source[sitor] = shcntx->shellcommand->arguments[i];
                        sitor++;
                    }
                    if (shcntx->shellcommand->arguments[i] == ' ') {
                        done_with_base = 1;
                    }
                } else {
                    // process the arguments
                    if ((shcntx->shellcommand->arguments[i] != '\0')) {
                        dest[ditor] = shcntx->shellcommand->arguments[i];
                        ditor++;
                    }
                }
            }

            source[sitor] = '\0';
            dest[ditor] = '\0';

            char *source_cmd = malloc(sitor + 1);
            strcpy(source_cmd, source);

            char *dest_cmd = malloc(ditor + 1);
            strcpy(dest_cmd, dest);


            int ln_result = link(source_cmd, dest_cmd);
            if (ln_result < 0) {
                perror("cannot link files: check file permissions, (source/dest) paths");
            }

        } else {
            printf("ERROR - Can't link without source/destination\n");
        }

        // use the link command
        retVal = 1;
    }


    if (strcmp(shcntx->shellcommand->base_command, "rm") == 0) {

        if (shcntx->shellcommand->arguments != NULL || shcntx->shellcommand->arguments != '\0') {

            int unlink_result = unlink(shcntx->shellcommand->arguments);
            if (unlink_result < 0) {
                perror("cannot rm files: check file permissions, paths");
            }

        } else {
            printf("ERROR - Can't link without source/destination\n");
        }

        // use the link command
        retVal = 1;
    }

    if (strcmp(shcntx->shellcommand->base_command, "fg") == 0) {
        if (shcntx->shellcommand->arguments != NULL || shcntx->shellcommand->arguments != '\0') {

            jobsllist *target;
            jobsllist *l = jobslist;
            while (l != NULL) {
                if ((l->job->id != 0) && (l->job->id == atoi(shcntx->shellcommand->arguments))) {
                    l->job->state = 1;
                    target = l;
                }
                l = l->next;
            }

            // traps the current running process in the shell
            int maxRetry = 3;
            int currRetry = 0;
            kill(target->job->pid, SIGCONT);

            while (currRetry < maxRetry) {

                while (read(target->job->readpipe, (void *) target->job->process_stdout_read_address, 1) == 1) {
                    write(STDOUT_FILENO, (void *) target->job->process_stdout_read_address, 1);
                    currRetry = maxRetry;
                }
                currRetry++;
            }

        }
        retVal = 1;
    }

    if (strcmp(shcntx->shellcommand->base_command, "bg") == 0) {
        if (shcntx->shellcommand->arguments != NULL || shcntx->shellcommand->arguments != '\0') {


            jobsllist *l = jobslist;
            while (l != NULL) {
                if ((l->job->id != 0) && (l->job->id == atoi(shcntx->shellcommand->arguments))) {
                    l->job->state = 1;
                    // continue the background jobs
                    l->job->state = 2;
                    kill(l->job->pid, SIGCONT);
                }
                l = l->next;
            }
        }
        retVal = 1;
    }

    return retVal;
}

const int TOO_MANY_INPUT_REDIRECTS_IN_1_LINE = 1000;
const int NO_REDIRECTION_FILE_SPECIFIED = 2000;
const int TRIPLE_OR_MORE_GREATER_THAN_SYMBOLS = 3000;
const int NO_COMMAND = 4000;

int shellCommandErrorsExist(shellcontext *shellcontext) {
    int response = 0;

    // check if >>> or more exist in a line
    if (shellcontext->triple_or_more_greater_than_symbol_errors) {
        response = TRIPLE_OR_MORE_GREATER_THAN_SYMBOLS;
    }

    // check for too many redirects
    if ((response == 0) && (shellcontext->less_than_count > 1)) {

        response = TOO_MANY_INPUT_REDIRECTS_IN_1_LINE;
    }

    // check for valid input file
    shellcommand *l = shellcontext->shellcommand;
    while ((response == 0) && (l->next != NULL)) {
        // check for redirect
        if ((l->proceeding_special_character == LESS_THAN_SYMBOL) &&
            (l->next != NULL)) {
            // check for valid input not ""
            if (l->next->command == '\0') {
                response = NO_REDIRECTION_FILE_SPECIFIED;
            }
        }
        l = l->next;
    }

    // check for valid command
    shellcommand *ll = shellcontext->shellcommand;
    while ((response == 0) && (ll->next != NULL)) {
        if ((ll->proceeding_special_character == GREATER_THAN_SYMBOL) &&
            (ll->command[0] == '\0')) {
            // ERROR ERROR - No command.
            response = NO_COMMAND;
        }
        ll = ll->next;
    }
    return response;
}

static jobsllist shelljobs[0];

// handles signals to control background and foreground jobs
void signal_handler(int action) {

    if (action == SIGCHLD) {
        pid_t pid;
        int status;
        while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
            removeJobFromJobsListByPID(shelljobs, pid);
        }
    }

    if (action == SIGTSTP) { // burned 2hr on SIGTSTP v. SIGSTOP
        jobsllist *l = shelljobs;
        while (l != NULL) {
            if ((l->job->id != 0) && (l->job->state == RUNNING_FOREGROUND)) {
                l->job->state = STOPPED_BACKGROUND;
                kill(l->job->pid, SIGSTOP);
            }
            l = l->next;
        }
    }

    if (action == SIGINT) {
        jobsllist *l = shelljobs;
        while (l != NULL) {
            if ((l->job->id != 0) && (l->job->state == RUNNING_FOREGROUND)) {
                // JOB STATE WILL BE HANDLED WHEN SIGCHLD IS CALLED ON KILL
                kill(l->job->pid, SIGCONT);
            }
            l = l->next;
        }
    }
}


int main(int argc, char **argv, char **envp) {

    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGTSTP, &sa, NULL) == -1) {
        perror("Error SIGTSTP handler");
        exit(EXIT_FAILURE);
    }

    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("Error SIGCHLD handler");
        exit(EXIT_FAILURE);
    }

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Error SIGINT handler");
        exit(EXIT_FAILURE);
    }

    // set the starting dir
    chdir(getenv("HOME"));

    char command[MAX_READ_SIZE];

    // add the shell to the jobs list
    addJobsListJob(shelljobs, createJob(getpid(), NULL, -1, RUNNING_FOREGROUND, "/bin/DPUShell"));

    // start the event shell
    while (1) {

        /// clean buffer
        for (int i = 0; i < MAX_READ_SIZE; i++) command[i] = 0;

#ifndef NOPROMPT
        char cwd[1024];
        getcwd(cwd, sizeof(cwd));
        printf("[%s]> ", cwd);
        fflush(stdout);
#endif

        // get user input
        if (!dpuread(command)) return 0;


        // clean input and verify command is not nothing
        command[strlen(command) - 1] = 0; // replace \n w/ \0

        if (strlen(command) == 0) continue;

        // get the shell context and process command for execution
        shellcontext *shcntx = processCommand(command);

        //listShellCommands(shcntx->shellcommand);

        int errors_exist = 0;

        switch (shellCommandErrorsExist(shcntx)) {

            case 1000: //TOO_MANY_INPUT_REDIRECTS_IN_1_LINE
                printf("ERROR - Can’t have two input redirects on one line\n");
                errors_exist = 1;
                break;
            case 2000: // NO_REDIRECTION_FILE_SPECIFIED
                printf("ERROR - No redirection file specified\n");
                errors_exist = 1;
                break;
            case 3000: // TRIPLE_OR_MORE_GREATER_THAN_SYMBOLS
                printf("ERROR - Cannot have >>> or more [>]\n");
                errors_exist = 1;
                break;
            case 4000: // NO_COMMAND
                printf("ERROR - No command\n");
                errors_exist = 1;
                break;
        }

        // check for builtin shell commands
        if (!errors_exist && !isBuiltinShellCommand(shelljobs, shcntx)) {

            // create pipes for inter process comm.
            int stdinPipe[2];
            int stdoutPipe[2];

            int child_pid;
            char processOutput;
            int exec_result_code;

            if (pipe(stdinPipe) < 0) {
                perror("error creating stdin pipe");
                return -1;
            }
            if (pipe(stdoutPipe) < 0) {
                close(stdinPipe[PIPE_READ]);
                close(stdinPipe[PIPE_WRITE]);
                perror("error creating stdout pipe");
                return -1;
            }

            child_pid = fork();
            if (0 == child_pid) { //child

                // bypass stdinlogic check
                int bypassStdinLogicCheck = 0;
                // logic for dealing with redirects

                if ((shcntx->shellcommand->next != NULL) &&
                    (shcntx->shellcommand->proceeding_special_character == GREATER_THAN_SYMBOL) &&
                    (shcntx->shellcommand->next->next != NULL) &&
                    (shcntx->shellcommand->next->proceeding_special_character == LESS_THAN_SYMBOL)) {
                    // handle the [  sort>out.txt<file.txt ] edge case

                    int fin = open(shcntx->shellcommand->next->next->command, O_RDONLY);
                    if (dup2(fin, STDIN_FILENO) == -1) exit(errno);
                    close(fin);
                    // output file is set below
                    bypassStdinLogicCheck = 1;
                }

                if (!bypassStdinLogicCheck){
                    if ((shcntx->shellcommand->next != NULL) &&
                        (shcntx->shellcommand->proceeding_special_character == LESS_THAN_SYMBOL)) {
                        int fin = open(shcntx->shellcommand->next->command, O_RDONLY);
                        if (dup2(fin, STDIN_FILENO) == -1) exit(errno);
                        close(fin);
                    } else {
                        if (dup2(stdinPipe[PIPE_READ], STDIN_FILENO) == -1) exit(errno);
                    }
                }

                // handle the [command > file.txt] case
                if (shcntx->shellcommand->next != NULL &&
                    shcntx->shellcommand->proceeding_special_character ==
                    GREATER_THAN_SYMBOL) { // write/overrwrite file

                    // open up a file with the correct permissions
                    int fout = open(shcntx->shellcommand->next->command, O_WRONLY | O_CREAT | O_TRUNC, 06666);
                    if (dup2(fout, STDOUT_FILENO) == -1) exit(errno);
                    if (dup2(fout, STDERR_FILENO) == -1) exit(errno);
                    close(fout);
                } else if (shcntx->shellcommand->next != NULL &&
                           shcntx->shellcommand->proceeding_special_character ==
                           DOUBLE_GREATER_THAN_SYMBOL) { // append to file
                    // open up a file to append to
                    int fout = open(shcntx->shellcommand->next->command, O_WRONLY | O_APPEND);
                    if (dup2(fout, STDOUT_FILENO) == -1) exit(errno);
                    if (dup2(fout, STDERR_FILENO) == -1) exit(errno);
                    close(fout);
                } else { // write to stdout
                    if (dup2(stdoutPipe[PIPE_WRITE], STDOUT_FILENO) == -1) exit(errno);
                    if (dup2(stdoutPipe[PIPE_WRITE], STDERR_FILENO) == -1) exit(errno);
                }

                // close the parent pipes
                close(stdinPipe[PIPE_READ]);
                close(stdinPipe[PIPE_WRITE]);
                close(stdoutPipe[PIPE_READ]);
                close(stdoutPipe[PIPE_WRITE]);

                // split the arguments based on the whitespace
                int itor = 0;
                int next_arr_ele = 0;

                char *argv[100];
                char tmpbuff[100];

                for (int i = 0; i <= strlen(shcntx->shellcommand->command); i++) {

                    if (((shcntx->shellcommand->command[i] == ' ') ||
                         (shcntx->shellcommand->command[i] == '\0')) && itor != 0) {
                        tmpbuff[itor] = '\0';
                        argv[next_arr_ele] = malloc(itor + 1);
                        strcpy(argv[next_arr_ele], tmpbuff);
                        next_arr_ele++;
                        itor = 0;
                    } else {
                        tmpbuff[itor] = shcntx->shellcommand->command[i];
                        itor++;
                    }
                }
                argv[next_arr_ele] = NULL;
                exec_result_code = execvp(argv[0], argv);

                // exit with error code, if reaches this point
                exit(exec_result_code);
            } else if (child_pid > 0) {
                // close unused file descriptors, these are for child only
                close(stdinPipe[PIPE_READ]);
                close(stdoutPipe[PIPE_WRITE]);

                // create job, store the process output address to read it
                // this allows us to support bg/fg jobs
                job *newjob = createJob(child_pid, &processOutput, stdoutPipe[PIPE_READ], RUNNING_FOREGROUND, command);
                addJobsListJob(shelljobs, newjob);

                while (read(stdoutPipe[PIPE_READ], (void *) newjob->process_stdout_read_address, 1) == 1) {
                    write(STDOUT_FILENO, (void *) newjob->process_stdout_read_address, 1);
                }
            }

            if (strcmp(shcntx->shellcommand->base_command, "exit") == 0) {
                exit(0);
            }

            // clean up allocated commands
            shellcommand *t = shcntx->shellcommand->next;
            while (t != NULL) {
                shellcommand *i = t->next;
                free(t);
                t = i;
            }
            free(shcntx);
        }
    }
    return 0;
}


