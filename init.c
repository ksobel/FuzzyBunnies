// *** CONSTANTS ***
#define TOTAL_NUM_PROC 12 //total number of processes, will change
#define TOTAL_NUM_IPROC 3 //total number of i-processes

#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>

#include "rtx.h"
#include "kbcrt.h"

// *** FUNCTION TO CLEAN UP PARENT PROCESSES***
void parent_die(int signal)
{
    cleanup();
    printf("\n\nSignal %i Received.   Leaving RTOS ...\n", signal);
    exit(0);
}

// ***CLEAN UP ROUTINE***
// routine to clean up things before terminating main program
// This stuff must be cleaned up or we have child processes and shared
// memory hanging around after the main process terminates
void cleanup() {
    // terminate child process(es)
    kill(in_pid, SIGINT);
    kill(out_pid, SIGINT);

    //******* KEYBOARD ********
    // remove shared memory segment and do some standard error checks
    k_status = munmap(k_mmap_ptr, k_bufsize);
    if (k_status == -1) {
        printf("Bad Keyboard munmap during cleanup\n");
    }
    
    // close the temporary mmap file 
    k_status = close(k_fid);
    if (k_status == -1) {
        printf("Bad close of temporary Keyboard mmap file during cleanup\n");
    };
    
    // unlink (i.e. delete) the temporary mmap file
    k_status = unlink(k_sfilename);
    if (k_status == -1) {
        printf("Bad unlink during Keyboard clean up\n");
    }
    
    //******* CRT ********
    // remove shared memory segment and do some standard error checks
    c_status = munmap(c_mmap_ptr, c_bufsize);
    if (c_status == -1) {
        printf("Bad CRT munmap during clean up\n");
    }
    
    // close the temporary mmap file 
    c_status = close(c_fid);
    if (c_status == -1) {
        printf("Bad close of temporary CRT mmap file during cleanup\n");
    };
    
    // unlink (i.e. delete) the temporary mmap file
    c_status = unlink(c_sfilename);
    if (c_status == -1) {
        printf("Bad unlink during CRT claeanup.\n");
    }
}

// *** CREATE PCB QUEUE FUNCTION ***
PCB_Q* create_Q( )
{
      // create the queue pointer
      PCB_Q *p;
      p = (struct pcbq *) malloc(sizeof (struct pcbq));
      
      // if the pointer is not NULL
      if (p){
        p->head = NULL;
        p->tail = NULL;
      }
        return p;     
}


// *** CREATE ENV QUEUE FUNCTION ***
env_Q* create_env_Q( )
{
      // create the queue pointer
      env_Q *bob;
      bob = (struct envQ *) malloc(sizeof (struct envQ));
      
      // if the pointer is not NULL
      if (bob){
        bob->head = NULL;
        bob->tail = NULL;
      }
        return bob;
}        


// *** INITIALIZE QUEUES ***
int init_queues( )
{
    // ready queue: Prio = 0
     ready_q_priority0 = create_Q();
     
     if(ready_q_priority0)
          printf("Priority Queue 0 Created\n");
     
     else {
          printf("Error Creating Priority Queue 0\n");
          return 0;
     }
          
     // ready queue: Prio = 1
     ready_q_priority1 = create_Q();
     
     if(ready_q_priority1)
          printf("Priority Queue 1 Created\n");
     
     else {
          printf("Error Creating Priority Queue 1\n");
          return 0;
     }
          
     // ready queue: Prio = 2
     ready_q_priority2 = create_Q();
     
     if(ready_q_priority2)
          printf("Priority Queue 2 Created\n");
     
     else {
          printf("Error Creating Priority Queue 2\n");
          return 0;
     }
          
     // ready queue: Prio = 3
     ready_q_priority3 = create_Q();
     
     if(ready_q_priority3)
          printf("Priority Queue 3 Created\n");
     
     else {
          printf("Error Creating Priority Queue 3\n");
          return 0;
     }
     
     // Blocked On Receive queue
     /*blocked_on_receive = create_Q(); This is not necessary
     
     if(blocked_on_receive)
          printf("Blocked on Receive Queue Created\n");
     
     else {
          printf("Error Creating Blocked on Envelope Queue\n");
          return 0;
     }*/
          
     // Blocked On Envelope queue
     blocked_on_envelope = create_env_Q();
     
     if(blocked_on_envelope)
          printf("Blocked on Envelope Queue Created\n");
     
     else {
          printf("Error Creating Blocked on Envelope Queue\n");
          return 0;
     }
          
     // Free Envelope Queue
     envelope_q = create_env_Q();
     
     if(envelope_q)
          printf("Free Envelope Queue Created\n");
     
     else {
          printf("Error Creating Envelope Queue\n");
          return 0;
     }
          
     return 1;
}


// *** INITIALIZE Non-I PROCESSES ****
int init_processes ( )
{
    // read in file for itable
    FILE* itablefile;
    itablefile = fopen("itable.txt", "r");

    // initialize variables used for reading from table
    int itable[TOTAL_NUM_PROC][2] = 0, pid = 3, priority = 3, i = 0;
    
    // setting up PIDs and Prioritys
    for (i = 0; i < TOTAL_NUM_PROC; i++) {
        
        // reading PID
        if (fscanf(itablefile, "%d", &pid))
            itable[i][0] = pid;
        
        else {
            printf("Error, initialization table missing PID for process ", i, "\n");
            return 0;
        }
        
        // reading Priority
        if (fscanf(itablefile, "%d", &priority))
            itable[i][1] = priority; //ALL PRIORITIES ARE SET TO ZERO FOR NOW. FIX THIS.
        
        else {
            printf("Error, initialization table missing Priority for process ", i, "\n");
            return 0;
        }
    }
    
    // close file for itable
    fclose(itablefile);
    
    // 
    int j;
    for (j = 0; j < TOTAL_NUM_PROC; j++) {
        
        // create temp pcb struct to put on appropriate queue
        struct pcb* new_pcb = (struct pcb *) malloc(sizeof (struct pcb));
        
        if (new_pcb){
            
            // set the PID for the appropriate process from the table
            new_pcb->pid = itable[i][0];
            
            // set the Priority for the appropriate process from the table
            new_pcb->priority = itable[i][1];
            
            // set ???? for the appropriate process from the table
            new_pcb->SP = NULL; //FOR CONTEXT SWITCHING. TO BE CHANGED LATER.
            
            // set process counter for the appropriate process from the table
            //new_pcb->PC = itable[i][2]; // WHAT IS THIS?!
            
            // set all processes to the READY state
            strcpy(new_pcb->state, "READY");
            
            // initialize the 'next' pointer (for queues) to NULL
            new_pcb->p = NULL;
            
            // initialize the process' receiving queue
            new_pcb->receive_msg_Q = create_env_Q();
            
            // create a pointer to the pcb, based on its PID, and save it in the array
            pointer_2_PCB[TOTAL_NUM_IPROC + j] = new_pcb;
            
            // enqueue the process on the appropriate ready queue
            if (new_pcb->priority == 0)
                PCB_ENQ(new_pcb, ready_q_priority0);
            
            else if (new_pcb->priority == 1)
                PCB_ENQ(new_pcb, ready_q_priority1);
            
            else if (new_pcb->priority == 2)
                PCB_ENQ(new_pcb, ready_q_priority2);
            
            else if (new_pcb->priority == 3)
                PCB_ENQ(new_pcb, ready_q_priority3);
            
            else {
                printf("Error, invalid priority for process ", j, "\n");
                return 0;
            }
        }
        
        // if the PCB pointer is created as NULL
        else{
            return 0;
        }
        
    }
   
   // if we get here, success!
   return 1;
}

int init_env()
{
     int i;
     for(i = 0; i < 128; i++)
     {
        // create temp env to be enqueued
        struct msgenv* new_env = (struct msgenv *) malloc (sizeof (struct msgenv));
        
        // if the new_env is not created properly
        if (!new_env)
            return 0;
        
        // initialize env parameters
        new_env->p = NULL;
        new_env->sender_id = -1; //setting the id to an int of -1 just for initialize
        new_env->target_id = -1; //setting the id to an int of -1 just for initialize
        
        struct msgenv* new_env->msg_type = (char *) malloc (sizeof (SIZE)); //initialize the character array pointer
        // if the msg_type pointer is not created properly
        if (!new_env->msg_type)
            return 0;
        
        struct msgenv* new_env->msg_text = (char *) malloc (sizeof (SIZE)); //initialize the character array pointer
        // if the msg_text pointer is not created properly
        if (!new_env->msg_text)
            return 0;
        
        //new_env->msg_type ; 
        //new_env->msg_text ;
        
        // enqueue the new env on the free env queue
        env_ENQ(new_env, envelope_q);
      }
     
      // if everything worked okay
      return 1;
}

int init_i_processes()
{
     //finish reading in file for iproctable
     int k; 
     for (k = 0; k < TOTAL_NUM_IPROC; k++)
     {
         struct pcb* new_pcb = (struct pcb *) malloc (sizeof (struct pcb));
         
         // if the PCB pointer is cool
         if (new_pcb){
             
             new_pcb->priority = -1;
             
             new_pcb->SP = NULL; //FOR CONTEXT SWITCHING. TO BE CHANGED LATER.
             
             new_pcb->pid = k;
             
             //new_pcb->PC = itable[k][1];
             strcpy(new_pcb->state,"READY"); //This is how you set the state
             
             new_pcb->p = NULL;
             
             new_pcb->receive_msg_Q = create_env_Q();
             
             pointer_2_PCB[k] = new_pcb;//This code creates a pointer to the pcb, based on its pid
             
         }
         
         // if the PCB pointer needs to go back to school
         else
             return 0;
     }
     
     // if everything jives
     return 1;
}

void kb_crt_start(){
    /* FORKING THE KEYBOARD  */
  
    /* Create a new mmap file for read/write access with permissions restricted
   to owner rwx access only */
    k_fid = open(k_sfilename, O_RDWR | O_CREAT | O_EXCL, (mode_t) 0755);
    if (k_fid < 0) {
        printf("Bad open of mmap file <%s> %i\n", k_sfilename, k_fid);
        exit(0);
    };

    // make the file the same size as the buffer 
    k_status = ftruncate(k_fid, k_bufsize);
    if (k_status) {
        printf("Failed to ftruncate the file <%s>, status = %d\n", k_sfilename, k_status);
        exit(0);
    }
   
    // pass parent's process id and the file id to child
    char k_childarg1[20], k_childarg2[20]; // arguments to pass to child process(es)
    int k_mypid = getpid(); // get current process pid
    sprintf(k_childarg1, "%d", k_mypid); // convert to string to pass to child
    sprintf(k_childarg2, "%d", k_fid);   // convert the file identifier
    
    // create the keyboard reader process
    // fork() creates a second process identical to the current process,
    // except that the "parent" process has in_pid = new process's ID,
    // while the new (child) process has in_pid = 0.
    // After fork(), we do execl() to start the actual child program.
    // (see the fork and execl man pages for more info)
    
    in_pid = fork();
    if (in_pid == 0)	// is this the child process ?
    {
            execl("./kbd_child", "keyboard", k_childarg1, k_childarg2, (char *)0);
            // should never reach here
            fprintf(stderr,"Can't exec keyboard, errno %d\n",errno);
            cleanup();
            exit(1);
    };
    // the parent process continues executing here

    // sleep for a second to give the child process time to start
    sleep(1);
    
    // allocate a shared memory region using mmap 
    // the child process also uses this region

    k_mmap_ptr = mmap((caddr_t)0,   // Memory location, 0 lets O/S choose
		    k_bufsize + 1,              // How many bytes to mmap -> size + 1 cause flag is at the end
		    PROT_READ | PROT_WRITE, // Read and write permissions
		    MAP_SHARED,    // Accessible by another process
		    k_fid,           // the file associated with mmap
		    (off_t) 0);    // Offset within a page frame

    if (k_mmap_ptr == MAP_FAILED){
        printf("Parent's memory map has failed, about to quit!\n");
	parent_die(0);  // do cleanup and terminate
    };

    // create the shared memory pointer
    in_mem_p = (struct inbuf *) malloc(sizeof (struct inbuf));

    // if the pointer is created successfully
    if (in_mem_p){

        in_mem_p->indata = (char *) k_mmap_ptr;
          // pointer to shared memory
          // we can now use 'in_mem_p' as a standard C pointer to access the created shared memory segment

        // now start doing whatever work you are supposed to do
        // in this case, do nothing; only the keyboard handler will do work

        // link the flag to the end of the buffer and set it 
        in_mem_p->ok_flag = &in_mem_p->indata[k_bufsize + 1];
        *in_mem_p->ok_flag = 0;
     }
    
    // if the shared memory pointer is stupid
    else
        printf("KB shared memory pointer not initialized\n");
    
    // FORKING THE CRT  
    
    // Create a new mmap file for read/write access with permissions restricted to owner rwx access only 
   
    c_fid = open(c_sfilename, O_RDWR | O_CREAT | O_EXCL, (mode_t) 0755);
    if (c_fid < 0) {
        printf("Bad open of mmap file <%s>\n", c_sfilename);
        exit(0);
    };

    // make the file the same size as the buffer 
    c_status = ftruncate(c_fid, c_bufsize);
    if (c_status) {
        printf("Failed to ftruncate the file <%s>, status = %d\n", c_sfilename, c_status);
        exit(0);
    }
    
    // pass parent's process id and the file id to child
    char c_childarg1[20], c_childarg2[20]; // arguments to pass to child process(es)
    int c_mypid = getpid(); // get current process pid
    sprintf(c_childarg1, "%d", c_mypid); // convert to string to pass to child
    sprintf(c_childarg2, "%d", c_fid);   // convert the file identifier
    
    // create the CRT output process
    // fork() creates a second process identical to the current process,
    // except that the "parent" process has in_pid = new process's ID,
    // while the new (child) process has in_pid = 0.
    // After fork(), we do execl() to start the actual child program.
    // (see the fork and execl man pages for more info)
    out_pid = fork();
    if (out_pid == 0)	// is this the child process ?
    {
            execl("./crt_child", "crt", c_childarg1, c_childarg2, (char *)0);
            // should never reach here
            fprintf(stderr,"Can't exec crt, errno %d\n",errno);
            cleanup();
            exit(1);
    };
    // the parent process continues executing here

    // sleep for a second to give the child process time to start
    sleep(1);
    
    // allocate a shared memory region using mmap 
	// the child process also uses this region

    c_mmap_ptr = mmap((caddr_t)0,   // Memory location, 0 lets O/S choose 
		    c_bufsize + 1,              // How many bytes to mmap 
		    PROT_READ | PROT_WRITE, // Read and write permissions
		    MAP_SHARED,    // Accessible by another process 
		    c_fid,           // the file associated with mmap 
		    (off_t) 0);    // Offset within a page frame 

    if (c_mmap_ptr == MAP_FAILED){
        printf("Parent's memory map has failed, about to quit!\n");
	parent_die(0);  // do cleanup and terminate
    };
	
    // create the shared memory pointer
    out_mem_p = (struct outbuf *) malloc(sizeof (struct outbuf));

    // if the pointer rocks out
    if (out_mem_p){

        out_mem_p->outdata = (char *) c_mmap_ptr;   // pointer to shared memory
      // we can now use 'in_mem_p' as a standard C pointer to access the created shared memory segment

    // now start doing whatever work you are supposed to do
    // in this case, do nothing; only the keyboard handler will do work
    
    // link the flag to the end of the shared memory and set it
    out_mem_p->oc_flag = &out_mem_p->outdata[c_bufsize + 1];
    *out_mem_p->oc_flag = 0;
    }
    
    // if the shared pointer needs help
    else
        printf("CRT shared memory pointer not initialized\n");
}

/*******************************************************************/
// ***** THE MAIN MAIN MAIN RTOS FUNCTION *****
int main ()
{
        // if init_queues returned 1
        if (init_queues())
                printf("Initialized queues correctly\n");        
        // if init_queues dropped the ball
        else {
                printf("Error, queue initialization failed!!!\n");
                exit;
        }
        
        // if init_env returned 1
        if (init_env())
                printf("Initialized envelopes correctly\n");        
        // if init_env is dumb
        else {
                printf("Error, envelope initialization failed!!!\n");
                exit;
        }
        
        // if init_processes returned 1
        if (init_processes())
                printf("Initialized processes correctly\n");
        // if init_processes failed
        else {
                printf("Error, process initialization failed!!!\n");
                exit;
        }
        
        // if init_i_processes returned 1
        if (init_i_processes())
                printf("Initialized I-processes correctly\n");
        // if init_-_processes failed
        else {
                printf("Error, i-process initialization failed!!!\n");
                exit;
        }
    
        // set the current process to whatever comes first
        // ***** should be the first process in the first ready queue? does that mean we can just hardcode this since it will be the same every time?
        curr_process = convert_PID(4);

        // ***** why are we doing this???
        strcpy(curr_process->state,"NEVER_BLK_PROC");

        
        // *****CODE FROM HERE TO THE BOTTOM WAS TAKEN FROM DEMO.C*****

        // catch signals so we can clean up everything before exiting
        // signals defined in /usr/include/signal.h
        sigset(SIGINT,parent_die);	// catch kill signals 
        sigset(SIGBUS,parent_die);	// catch bus errors
        sigset(SIGHUP,parent_die);		
        sigset(SIGILL,parent_die);	// illegal instruction
        sigset(SIGQUIT,parent_die);
        sigset(SIGABRT,parent_die);
        sigset(SIGTERM,parent_die);
        sigset(SIGSEGV,parent_die);	// catch segmentation faults
        sigset(SIGUSR1,kbd_iproc);
        sigset(SIGUSR2,crt_iproc);
        sigset(SIGALRM,timer_iproc); //Catch clock ticks

        // INITIALIZE KB AND CRT
        kb_crt_start();
        
        
        //Allocate memory for the wallclock structure
        struct clock* wallclock = (struct clock *) malloc (sizeof (struct clock));
        
        //Initialize and set the wallclock to 0
        if(wallclock) {
             wallclock->ss = 0;
             wallclock->mm = 0;
             wallclock->hh = 0;
        }
        else {
            printf("Error, wallclock initialization failed!!!\n");
            exit;
        }
        
        //Allocate memory for the systemclock structure
        struct clock* systemclock = (struct clock *) malloc (sizeof (struct clock));
        
        //Initialize and set the wallclock to 0
        if(systemclock) {
             systemclock->ss = 0;
             systemclock->mm = 0;
             systemclock->hh = 0;
        }
        else {
            printf("Error, system clock initialization failed!!!\n");
            exit;
        }
        
        
        //initialize the dummy pulse counter
        int pulse_counter = 0;
        
        //set a repeating alarm to send SIGALRM every 100000 usec, or 0.1sec
        int alarmstatus = ualarm(100000, 100000);
        //ualarm returns zero if functioning normally, otherwise return an error message
        if(alarmstatus != 0)
             printf("Error: Something is wrong with the system timer!\n");
        


        //*** BACK TO MAIN RTOS STUFF ***
        // **** this section should initialize anything else that needs to happen

        //The following functions are not necessary for the partial implementation, but will be for the full.
        //allocate system_clock = (struct clock *) malloc (sizeof (struct clock))
        //system_clock.ss = 0
        //system_clock.mm = 0
        //system_clock.hh = 0
            //release_processor( )
            //process_switch( )
        
        //printf("Welcome to the wonderful world of RTOS! Type (whatever) to see it go!"); 
       
        // code to make process P work
        printf("Type something then an end-of-line and it will be echoed by the superbly awesome Process P:\n");
        processP();
        
        // should never reach here, but in case we do, clean up after ourselves
        cleanup();
        exit(1);
}	
