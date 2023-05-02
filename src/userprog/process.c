#include <debug.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "userprog/gdt.h"      /* SEL_* constants */
#include "userprog/process.h"
#include "userprog/load.h"
#include "userprog/pagedir.h"  /* pagedir_activate etc. */
#include "userprog/tss.h"      /* tss_update */
#include "filesys/file.h"
#include "threads/flags.h"     /* FLAG_* constants */
#include "threads/thread.h"
#include "threads/vaddr.h"     /* PHYS_BASE */
#include "threads/interrupt.h" /* if_ */
#include "threads/init.h"      /* power_off() */

/* Headers not yet used that you may need for various reasons. */
#include "threads/synch.h"
#include "threads/malloc.h"
#include "lib/kernel/list.h"

#include "userprog/flist.h"
#include "userprog/plist.h"

/* HACK defines code you must remove and implement in a proper way */
#define HACK

/* This function is called at boot time (threads/init.c) to initialize
 * the process subsystem. */
void process_init(void)
{
}

/* This function is currently never called. As thread_exit does not
 * have an exit status parameter, this could be used to handle that
 * instead. Note however that all cleanup after a process must be done
 * in process_cleanup, and that process_cleanup are already called
 * from thread_exit - do not call cleanup twice! */
void process_exit(int status)
{
  process_list.content[thread_current()->pid]->exit_status = status;
}

/* Print a list of all running processes. The list shall include all
 * relevant debug information in a clean, readable format. */
void process_print_list()
{
}


struct parameters_to_start_process
{
  char* command_line;
  
  struct semaphore start_process_done;
  bool start_process_success;

  int parent_pid; //set by process_execute
  int pid; //set by start_process_
};

static void
start_process(struct parameters_to_start_process* parameters) NO_RETURN;

/* Starts a new proccess by creating a new thread to run it. The
   process is loaded from the file specified in the COMMAND_LINE and
   started with the arguments on the COMMAND_LINE. The new thread may
   be scheduled (and may even exit) before process_execute() returns.
   Returns the new process's thread id, or TID_ERROR if the thread
   cannot be created. */
int
process_execute (const char *command_line)
{
  char debug_name[64];
  int command_line_size = strlen(command_line) + 1;
  tid_t thread_id = -1;
  int  process_id = -1;

  /* LOCAL variable will cease existence when function return! */
  struct parameters_to_start_process arguments;

  debug("%s#%d: process_execute(\"%s\") ENTERED\n",
        thread_current()->name,
        thread_current()->tid,
        command_line);

  /* COPY command line out of parent process memory */
  arguments.command_line = malloc(command_line_size);
  strlcpy(arguments.command_line, command_line, command_line_size);


  strlcpy_first_word (debug_name, command_line, 64);
  // Initialize semaphor to 0 -> so process_execute waits for start_process
  sema_init(&arguments.start_process_done, 0);

  // Initialize parrent pid of new process to current pid
  arguments.parent_pid = thread_current()->pid;

  //Initialize pid of new process to -1 incase start_process fails
  arguments.pid = -1;
  
  /* SCHEDULES function `start_process' to run (LATER) */
  thread_id = thread_create (debug_name, PRI_DEFAULT,
                             (thread_func*)start_process, &arguments);

  if(thread_id == -1)
  {
    //Thread not created -> start_process never called -> no need to wait
    process_id = -1;

    debug("%s#%d: process_execute(\"%s\") reads UNSUCCESSFUL thread_create\n",
          thread_current()->name,
          thread_current()->tid,
          command_line);
  }
  else
  {
    //Wait for start_process to finnish using arguments and reporting if
    //executable program was loaded successfully.
    sema_down(&arguments.start_process_done);

    if(arguments.start_process_success)
    {
      process_id = arguments.pid;
    }
    else
    {      
      process_id = -1;
      
      debug("%s#%d: process_execute(\"%s\") reads UNSUCCESSFUL process_start\n",
            thread_current()->name,
            thread_current()->tid,
            command_line);
    }
  }

  /* WHICH thread may still be using this right now? */
  free(arguments.command_line);

  debug("%s#%d: process_execute(\"%s\") RETURNS %d\n",
        thread_current()->name,
        thread_current()->tid,
        command_line, process_id);

  /* MUST be -1 if `load' in `start_process' return false */
  return process_id;
}

/* ASM version of the code to set up the main stack. */
void *setup_main_stack_asm(const char *command_line, void *esp);

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (struct parameters_to_start_process* parameters)
{
  /* The last argument passed to thread_create is received here... */
  struct intr_frame if_;
  bool success;

  char file_name[64];
  strlcpy_first_word (file_name, parameters->command_line, 64);

  debug("%s#%d: start_process(\"%s\") ENTERED\n",
        thread_current()->name,
        thread_current()->tid,
        parameters->command_line);

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;

  success = load (file_name, &if_.eip, &if_.esp);

  debug("%s#%d: start_process(...): load returned %d\n",
        thread_current()->name,
        thread_current()->tid,
        success);

  if (success)
  {
    /* We managed to load the new program to a process, and have
       allocated memory for a process stack. The stack top is in
       if_.esp, now we must prepare and place the arguments to main on
       the stack. */

    /* A temporary solution is to modify the stack pointer to
       "pretend" the arguments are present on the stack. A normal
       C-function expects the stack to contain, in order, the return
       address, the first argument, the second argument etc. */

    // if_.esp -= 12; /* this is a very rudimentary solution */

    /* This uses a "reference" solution in assembler that you
       can replace with C-code if you wish. */
    if_.esp = setup_main_stack_asm(parameters->command_line, if_.esp);

    /* The stack and stack pointer should be setup correct just before
       the process start, so this is the place to dump stack content
       for debug purposes. Disable the dump when it works. */

//    dump_stack ( PHYS_BASE + 15, PHYS_BASE - if_.esp + 16 );

    //Insert process information in process_list
    struct process* ins = malloc(sizeof(struct process));
    strlcpy_first_word (ins->name, parameters->command_line, 64);
    ins->parent_pid = parameters->parent_pid;
    ins->exit_status = -1;
    sema_init(&ins->exit_status_ready, 0);
    ins->dead = false;
    ins->parent_dead = false;

    int insert_ret = plist_insert(ins);

    if(insert_ret == -1)
    {
      //process_list is full
      debug("%s#%d: start_process(\"%s\") Process list full!\n",
            thread_current()->name,
            thread_current()->tid,
            parameters->command_line);
      free(ins);
      parameters->start_process_success = false;
    }
    else
    {
      //Update pid in struct thread
      thread_current()->pid = insert_ret;

      //Report to process_execute that start_process was successful.
      parameters->start_process_success = true;
      parameters->pid = insert_ret;
    }
  }
  else
  {
    //Report to process_execute that start_process was unsuccessful.
    parameters->start_process_success = false;
  }

  debug("%s#%d: start_process(\"%s\") DONE PID: %d \n",
        thread_current()->name,
        thread_current()->tid,
        parameters->command_line, parameters->pid);
  if(success)
  {
    debug("succes\n");
  }
  else
  {
    debug("failure\n");
  }

  //start_process done using parameters and reporting if executable program was
  //loaded successfully.
  sema_up(&parameters->start_process_done);
  
  /* If load fail, quit. Load may fail for several reasons.
     Some simple examples:
     - File doeas not exist
     - File do not contain a valid program
     - Not enough memory
  */
  if ( ! success )
  {
    thread_exit ();
  }

  /* Start the user process by simulating a return from an interrupt,
     implemented by intr_exit (in threads/intr-stubs.S). Because
     intr_exit takes all of its arguments on the stack in the form of
     a `struct intr_frame', we just point the stack pointer (%esp) to
     our stack frame and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Wait for process `child_id' to die and then return its exit
   status. If it was terminated by the kernel (i.e. killed due to an
   exception), return -1. If `child_id' is invalid or if it was not a
   child of the calling process, or if process_wait() has already been
   successfully called for the given `child_id', return -1
   immediately, without waiting.

   This function will be implemented last, after a communication
   mechanism between parent and child is established. */
int
process_wait (int child_id)
{
  int status;
  struct thread *cur = thread_current ();

  debug("%s#%d: process_wait(%d) ENTERED\n",
        cur->name, cur->tid, child_id);
  
  /* Yes! You need to do something good here ! */
  struct process* child = plist_find(child_id);
  if(child == NULL || child->parent_pid != cur->pid)
  {
    status = -1;
  }
  else
  {
    sema_down(&child->exit_status_ready);
    status = child->exit_status;

    //child exit_status no longer needed -> remove child from plist 
    plist_remove(child_id);
    free(child);
    debug("PID: %d HARD DELETE in WAIT\n", child_id);
  }
  
  debug("%s#%d: process_wait(%d) RETURNS %d\n",
        cur->name, cur->tid, child_id, status);

  return status;
}

void process_list_cleanup(int exited_pid)
{
  struct process* exited = plist_find(exited_pid);
  debug("PID: %d ", exited_pid);
  if(exited == NULL)
  {
    //debug("PID: %d not found in plist: plist_cleanup exited.\n", exited_pid);
    debug("not found in plist: plist_cleanup exited.\n");
  }
  else
  {
    if(exited->parent_dead) // or exited->parent_pid == -1)
    {
      //exited and exited parent dead
      //remove exited and all affected that are no longer needed from plist 
      plist_remove(exited_pid);
      free(exited);
      debug("HARD DELETE\n");
    }
    else
    {
      //exited dead but exited parent alive -> update exited values
      exited->dead = true;
      sema_up(&exited->exit_status_ready);
      // parent can now read exited exit_status
      // exit status will be -1 if process crashed
      // exit status will be 0 if process exited succesfully
      // exit status will be >0 if process exited unsuccesfully
      debug("SOFT DELETE\n");
    }
    
    //update children processes and remove if no longer needed
    for(int i = 0; i < PLIST_SIZE; i++)
    {
      if(process_list.content[i] != NULL)
      {
        if(process_list.content[i]->parent_pid == exited_pid)
        {
          debug("CHILD_PID: %d parent deleted\n", i);
          process_list.content[i]->parent_dead = true;
          if(process_list.content[i]->dead)
          {
            //child now is dead and has dead parent -> remove recursively
            process_list_cleanup(i);
          }
        }
      }
    }
  }
}

/* Free the current process's resources. This function is called
   automatically from thread_exit() to make sure cleanup of any
   process resources is always done. That is correct behaviour. But
   know that thread_exit() is called at many places inside the kernel,
   mostly in case of some unrecoverable error in a thread.

   In such case it may happen that some data is not yet available, or
   initialized. You must make sure that nay data needed IS available
   or initialized to something sane, or else that any such situation
   is detected.
*/

void
process_cleanup (void)
{
  struct thread  *cur = thread_current ();
  uint32_t       *pd  = cur->pagedir;
  int status = -1;

  //close and free all open files in flist for process
  flist_cleanup(&cur->open_files);
  
  debug("%s#%d: process_cleanup() ENTERED PID: %d \n", cur->name, cur->tid, cur->pid);

  /* Later tests DEPEND on this output to work correct. You will have
   * to find the actual exit status in your process list. It is
   * important to do this printf BEFORE you tell the parent process
   * that you exit.  (Since the parent may be the main() function,
   * that may sometimes poweroff as soon as process_wait() returns,
   * possibly before the printf is completed.)
   */
  
  status = plist_find(cur->pid)->exit_status;
  printf("%s: exit(%d)\n", thread_name(), status);

  //update exited processes to dead and let waiting parents know child is dead 
  //remove and free all processes from plist that after this exit are not needed
  process_list_cleanup(cur->pid);

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  if (pd != NULL)
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
  debug("%s#%d: process_cleanup() DONE with status %d\n",
        cur->name, cur->tid, status);
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

