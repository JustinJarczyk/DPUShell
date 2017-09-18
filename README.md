# DPUShell https://github.com/JustinJarczyk/DPUShell


=============================================================================
# Assignment Deliverables

## Assignment 1 - Writing your own Shell

=============================================================================

## Project overview.

The DPUShell is a unix shell, it has similar functionality to the bash shell. 
It provides a way of interacting and manipulating the Filesystem to execute programs.
In addition to executing CLI Binaries, the DPUShell has the ability to redirect STDIN, STDOUT to files on the FS.
Job delegation and controls are supported. Please see the list of details below

Redirection Command list
- '<' # read
- '>' # write
- '>>' # append

Usage Examples
- < bar /bin/cat
- /bin/ls < foo
- /bin/bug -p1 -p2 foobar
- cat>bar<README

Builtin Commands
- cd {{dir}}
- ln {{src}} {{dest}}
- rm {{file}}
- exit
- jobs # lists all running jobs
- fg {{job id}} # bring a job to the foreground (example: fg 1 or fg 2) ** note no %1, %2 like in bash


## Assumptions made, if any.

- The maximum read/write buffer sizes are 1024
- Jobs can have 3 states, Foreground running, background running, background stopped
    - SIGINT will be propogated to the process groupt causing the child to revieve it
    - SIGTSTP should behave to stop the currently running foreground process if any
    - SIGCHLD will be given once a process has completed, which in turn should kill the child process
- Redirection Assumptions
    - Can’t have two input redirects on one line
    - When no redirection file is specified, returns an error
    - Cannot have >>> or more [>]
    - When redirection from nothing - should return an error
- Interprocess communication & pipe Assumptions
    - When dup2 is called to duplicate a filedescriptor, the original fd can be closed
- When the shell starts it will open up in the users home directory
- 

## Any Design decisions you had to make or issues you faced, in brief. 4. Summary (include any specific new learnings, if possible).

## Design Decisions
 

### Issues & Solutions

- Signals: First I tried to register signals using [signal(XXX, XXX);] however I was having trouble catching the signal
while the program reading the child processes input, I cam across this article https://stackoverflow.com/questions/31097058/sending-signal-from-parent-to-child 
and began registering signals via [sigact.sa_handler =]
    - I burned 2 hours with a typo in SIGTSTP and SIGSTOP
- I was having trouble redirecting I/O and being able to reconnect to a stopped/started process
    - This article helped me structure my fork and setup my READ/WRITE I/O pipes between the parent and the child : https://stackoverflow.com/questions/9405985/linux-3-0-executing-child-process-with-piped-stdin-stdout

### Program Execution

- Register Signal Handlers to "void signal_handler(int action)"
- cd to the logged in users home directory
- Create a base job for the shell "addJobsListJob(shelljobs, createJob(getpid(), NULL, -1, RUNNING_FOREGROUND, "/bin/DPUShell"))"
- Start the Event Loop
    - Print Prompt
    - Wait/Read user input
    - On user input "shellcontext *shcntx = processCommand(command);", process the command into the [struct shellcontext]
    - Check the command for any errors [int shellCommandErrorsExist(shellcontext *shellcontext)]
        - TOO_MANY_INPUT_REDIRECTS_IN_1_LINE ** cannot have more the 2 input redirects in one line
        - NO_REDIRECTION_FILE_SPECIFIED ** need to specify a redirection file
        - TRIPLE_OR_MORE_GREATER_THAN_SYMBOLS ** cannot have ">>>"
        - NO_COMMAND ** user must specify a command
    - Check if the command the user put in is a builtin shell command "int isBuiltinShellCommand(jobsllist *jobslist, shellcontext *shcntx)"
        - jobs ** lists all of the active jobs (excluds the base /bin/DPUSHell job)
        - cd ** cd's to a directory, added in ~ support
        - ln ** links a directory
        - rm ** removes a file
        - fg ** brings a stopped or running background job to the foreground (fg {{job id}}, output from jobs)
        - bg ** continues a job running in the background (bg {{job id}}, output from jobs)
        - exit ** exits the program
    - Create STDIN/STDOUT pipes for redirection of job contexts (background, foreground)
    - fork()
        - child
            - Setup & clone dup2 IO STDIN/STDOUT file descriptors for the child
            - Close parent file descriptors
            - Run the command [execvp(argv[0], argv)]
        - parent
            - Close child IO STDIN/STDOUT file descriptors for the child
            - Create a new job for the job list, with the child pid
            - Read childs STDIN
        
       
### Jobs & Foreground/Background

The shell controls jobs/contexts via Jobs [struct job] and a linked list [struct jobsllist] to hold the job.

Description: These structures where created to hold the job states and the contexts in which they operate 
to go between foreground, background, and stopped.    

- [struct job] ** Holds the information about a job
    - int id; ** Job ID, internal structure
    - int pid; ** Process ID of the forked job
    - int state; ** the state of the job (RUNNING_FOREGROUND, RUNNING_BACKGROUND, STOPPED_BACKGROUND)
    - char *command; ** the command being run
    - size_t process_stdout_read_address; ** the out file descriptor for the process generated by [dup2]
    - int readpipe; ** the read file descriptor for the process generated by [dup2]
- [struct jobsllist] ** linked list of jobsllist
    - job *job; ** holds the job
    - struct jobsllist *next; ** holds the next jobsllist

Methods
- job *createJob(int pid, char *output_address_pointer, int readpipe, int state, char *command)
    - Creates jobs to be inserted into the jobsllist
- void *removeJobFromJobsListByPID(jobsllist *jobs, int pid)
    - Removes a job from the jobsllist by pid
- void *listJobsListJobs(jobsllist *jobs)
    - Lists out all the jobs in the jobsllist
- void *addJobsListJob(jobsllist *jobslist, job *j)
    - Adds a job to the jobsllist
 
### Commands & Command Context

I chose to have the shellcommands represented as a linked list because it allowed the program to be extensible and
modular while being able to handle more complicated commands.

The [struct shellcontext] handles all of the metadata for [struct shellcommand] to be executed. 

Description: The shell context holds IO redirection sysmbol counts for later validation. 
The shell command is a linked list structure. Given the command "sort < abc -p1 -p2 > file" 
shellcommand data structure will be represented as "sort <" -> "abc -p1 -p2" -> "file".

- [struct shellcontext] 
    - int greater_than_count; ** count of '>'
    - int less_than_count; ** count of '<'
    - int triple_or_more_greater_than_symbol_errors; ** count of '>>'
    - struct shellcommand *shellcommand; ** holds the linkedlist shellcommand 

- [struct shellcommand] ** linked list of shellcommands, broken up in the description above 
    - char *command; ** the command with all arguments
    - char *base_command; ** the base command without the arguments
    - char *arguments; ** the arguments only
    - int proceeding_special_character; ** the following special character (NO_CHARACTER = 0, GREATER_THAN_SYMBOL = 1, LESS_THAN_SYMBOL = 2, DOUBLE_GREATER_THAN_SYMBOL = 3)
    - struct shellcommand *next; ** holds the next shell command

Methods
- void *listShellCommands(shellcommand *commands) ** used to debug the shellcommands linked list
- shellcontext *processCommand(char *input) ** processes the raw input into a shellcontext, and applies all of the rules to make a shellcommand 


## Source Code in Appendix.

Online References
- Exec
    - https://stackoverflow.com/questions/3703013/what-can-cause-exec-to-fail-what-happens-next
    - https://stackoverflow.com/questions/7630551/using-a-new-path-with-execve-to-run-ls-command
    - https://stackoverflow.com/questions/21558937/i-do-not-understand-how-execlp-works-in-linux
    - https://stackoverflow.com/questions/22883970/exec-system-call-without-using-the-path
- Environment Variables
    - https://stackoverflow.com/questions/2085302/printing-all-environment-variables-in-c-c
    - https://stackoverflow.com/questions/14290556/running-executables-present-in-the-path-environment-using-execve ** environment variables w/ exec
- String Handling
    - https://stackoverflow.com/questions/11198604/c-split-string-into-an-array-of-strings
    - https://stackoverflow.com/questions/19803437/strtok-to-separate-all-whitespace
    - https://stackoverflow.com/questions/15000544/how-to-get-the-string-size-in-bytes
    - https://stackoverflow.com/questions/308695/how-do-i-concatenate-const-literal-strings-in-c
    - https://stackoverflow.com/questions/8465006/how-do-i-concatenate-two-strings-in-c
    - https://stackoverflow.com/questions/18468229/concatenate-two-char-strings-in-a-c-program
    - https://stackoverflow.com/questions/20026727/how-to-convert-integers-to-characters-in-c
    - https://stackoverflow.com/questions/25838628/copying-string-literals-in-c-into-an-character-array ** copy string into [char *]
    - https://stackoverflow.com/questions/1071542/in-c-check-if-a-char-exists-in-a-char-array
    - https://stackoverflow.com/questions/17129070/c-to-check-if-a-char-in-a-string-belongs-in-an-array
    - https://stackoverflow.com/questions/122616/how-do-i-trim-leading-trailing-whitespace-in-a-standard-way ** trim whitespace in processCommand
    - https://stackoverflow.com/questions/308695/how-do-i-concatenate-const-literal-strings-in-c
    - https://stackoverflow.com/questions/8465006/how-do-i-concatenate-two-strings-in-c
    - https://stackoverflow.com/questions/10162152/how-to-work-with-string-fields-in-a-c-struct
- Reading/Writing STDIN/STDOUT, buffer flushing
    - https://stackoverflow.com/questions/1242974/write-to-stdout-and-printf-output-not-interleaved
    - https://stackoverflow.com/questions/3866217/how-can-i-make-the-system-call-write-print-to-the-screen
    - https://stackoverflow.com/questions/19769542/reading-from-file-using-read-function
    - https://stackoverflow.com/questions/15883568/reading-from-stdin
- FILE I/O
    - https://stackoverflow.com/questions/13691107/c-proper-way-to-close-files-when-using-both-open-and-fdopen
    - https://stackoverflow.com/questions/23092040/how-to-open-a-file-which-overwrite-existing-content
    - https://stackoverflow.com/questions/7136416/opening-file-in-append-mode-using-open-api
- Data Structures
    - https://stackoverflow.com/questions/19542170/string-array-declaration-in-c
    - https://stackoverflow.com/questions/15094834/check-if-a-value-exist-in-a-array ** helps check if value is in array
    - https://stackoverflow.com/questions/29522264/correct-way-to-use-malloc-and-free-with-linked-lists
    - https://stackoverflow.com/questions/21458327/c-correctly-using-malloc-for-linked-list ** memory management for ll
    - https://stackoverflow.com/questions/5797548/c-linked-list-inserting-node-at-the-end
    - https://stackoverflow.com/questions/6417158/c-how-to-free-nodes-in-the-linked-list
    - https://stackoverflow.com/questions/10162152/how-to-work-with-string-fields-in-a-c-struct
- Error Debugging/Handling
    - https://stackoverflow.com/questions/24317300/printf-causes-exc-bad-accesscode-exc-i386-gpflt-warning-and-freezes-at-runtime
    - https://stackoverflow.com/questions/9317529/when-does-printf-fail-to-print ** needed to flush the output
    - https://stackoverflow.com/questions/12450066/flushing-buffers-in-c ** buffer flush
    - https://stackoverflow.com/questions/16870059/printf-not-printing-to-screen
- Memory Management
    - https://stackoverflow.com/questions/23232114/exc-bad-access-error-in-c
    - https://stackoverflow.com/questions/4214314/get-a-substring-of-a-char
    - https://stackoverflow.com/questions/22699593/storing-address-of-a-character-to-an-integer-pointer
    - https://stackoverflow.com/questions/34904364/store-address-as-value-and-use-it-later-on-pointer
- Fork & Processes
    - https://stackoverflow.com/questions/7292642/grabbing-output-from-exec
    - https://stackoverflow.com/questions/2146602/how-to-get-return-value-from-child-process
    - https://stackoverflow.com/questions/9405985/linux-3-0-executing-child-process-with-piped-stdin-stdout
    - https://stackoverflow.com/questions/27306764/capturing-exit-status-code-of-child-process
    - https://stackoverflow.com/questions/2595503/determine-pid-of-terminated-process ** get the pid of an exited process
- Inter Process Communication
    - https://stackoverflow.com/questions/2605130/redirecting-exec-output-to-a-buffer-or-file
    - https://stackoverflow.com/questions/36908660/exec-cd-and-ls-and-fork-call-c
    - https://stackoverflow.com/questions/12645236/c-read-stdout-from-multiple-exec-called-from-fork
    - https://stackoverflow.com/questions/13206602/how-to-redirect-the-io-of-child-process-created-by-fork-and-uses-exec-functi
-  Signals
    - https://stackoverflow.com/questions/4217037/catch-ctrl-c-in-c
    - https://stackoverflow.com/questions/9937743/what-keyboard-signal-apart-from-ctrl-c-can-i-catch SIGSTP
    - https://stackoverflow.com/questions/2377811/tracking-the-death-of-a-child-process
    - https://stackoverflow.com/questions/5113545/enable-a-signal-handler-using-sigaction-in-c
- MISC
    - https://stackoverflow.com/questions/14069737/switch-case-error-case-label-does-not-reduce-to-an-integer-constant
    - https://stackoverflow.com/questions/11689912/label-does-not-reduce-to-an-integer-constant
    - https://stackoverflow.com/questions/2552416/how-can-i-find-the-users-home-dir-in-a-cross-platform-manner-using-c ** get the users home directory
    - https://stackoverflow.com/questions/16376892/c-using-chdir-function
    - https://stackoverflow.com/questions/41884685/implicit-declaration-of-function-wait ** solved compiler warning on wait() command
    - https://stackoverflow.com/questions/745152/what-is-the-maximum-size-of-buffers-memcpy-memset-etc-can-handle
    - https://stackoverflow.com/questions/2085302/printing-all-environment-variables-in-c-c
    
=============================================================================



## Setting up your development Environment

- OS : ubuntu-16.04.3-server-amd64.iso
- Download ISO URL : https://www.ubuntu.com/download/server
- Credentials
    - UN: root
    - PW: Test1234!
    - UN: ninja
    - PW: Test1234!
- Project directory is mounted in /srv under DPUShell

##OS Installation/Configuration Instructions

- During Installation Partition with LVM
- Install ssh server (either during installation or via apt-get)
    - apt-get install openssh-server 
- Setup root password (Test1234!)
- Setup SSH for root (/etc/ssh/sshd_config) -> PermitRootLogin yes
- As root install VirtualBox guest Additions ** needed for mounting shared directories 
    - apt-get update
    - apt-get install virtualbox-guest-dkms
    - apt-get install zlib1g-dev // this adds in the zconf.h header file, needed to have variable arguments (...)
    - apt-get install -y dkms build-essential linux-headers-generic linux-headers-$(uname -r)
    - follow this tutorial: http://www.techrepublic.com/article/how-to-install-virtualbox-guest-additions-on-a-gui-less-ubuntu-server-host/
        - To mount the CD start the virtual machine and go to devices within the host machine menu
    - mkdir /srv/DPUShell ** this will create a directory to mount the project to 

** reboot the server

##VirtualBox Setup (Version 5.1.26 r117224 (Qt5.6.2))

Setup Networking (varies per machine)
 
- Networking 
    - Enable Network adapter
    - Set Network adapter to "Bridged adapter"
    - Set Permiscuous Mode setting to Allow All
    - Set to cable connected
- Shared Folders
    - Add a shared folder (Folder path is the path of your project)
    - Folder Name is "DPUShell"
    - Select "Auto-mount" & "Make Permanent"
    - add the following entry to /etc/fstab "/media/sf_DPUShell /srv/DPUShell none defaults,bind 0 0"
        - ** That entry will automatically mount on startup, to test for correctness run "mount -a" 

# Test your OS/VMWare installation/configuration
- Reboot the VM 
- You should be ready to Develop on this project from your local machine
- Tests to make sure you setup your environment correctly
    - cd /srv/DPUShell
        - ls (do you see the contents for the project)
    - run ifconfig to get the IP address
    - try to ssh into the running vm from your local machine "ssh root@{{your vm ip address}}"
    - try to ssh into the running vm from your local machine "ssh ninja@{{your vm ip address}}"
   
   
# Setup your test shell for the 'ninja' user account

** you need cmake 3.8.2

- cd ~
- wget https://cmake.org/files/v3.8/cmake-3.8.2.tar.gz
- tar -xvf cmake-3.8.2.tar.gz
- cd cmake-3.8.2.tar.gz
- ./bootstrap
- make
- make install

--- 

# Setting up the ninja user
- run /srv/DPUShell/build/build_install.sh, to cmake, build and setup the existing ninja users shell

** for testing purposes, setup ssh key access for the ninja user saves you a lot of time by not having to enter in your password

# Development and Testing

- IDE: CLion (2017.2.2)
- You can either develop directly on the linux machine or develop locally via the shared directory in Virtualbox
- After coding to test your changes, run the /srv/DPUShell/build/build_install.sh script
    - This will build and deploy your shell as the default shell for the test user
- ssh into the vm as ninja@x.x.x.x and your shell will execute
- repeat the process







