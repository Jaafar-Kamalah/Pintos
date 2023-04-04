#include <stdio.h>
#include <syscall-nr.h>
#include "userprog/syscall.h"
#include "threads/interrupt.h"
#include "threads/thread.h"

/* header files you probably need, they are not used yet */
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/vaddr.h"
#include "threads/init.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "devices/input.h"

//Comment printf to remove debugging output in console
#define DBG(format, ...) printf(format, ##__VA_ARGS__)

static void syscall_handler (struct intr_frame *);

void
syscall_init (void)
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}


/* This array defined the number of arguments each syscall expects.
   For example, if you want to find out the number of arguments for
   the read system call you shall write:

   int sys_read_arg_count = argc[ SYS_READ ];

   All system calls have a name such as SYS_READ defined as an enum
   type, see `lib/syscall-nr.h'. Use them instead of numbers.
 */
const int argc[] = {
  /* basic calls */
  0, 1, 1, 1, 2, 1, 1, 1, 3, 3, 2, 1, 1,
  /* not implemented */
  2, 1,    1, 1, 2, 1, 1,
  /* extended, you may need to change the order of these two (plist, sleep) */
  0, 1
};

static void
syscall_handler (struct intr_frame *f)
{
  int32_t* esp = (int32_t*)f->esp;
  int32_t syscall_number = esp[0];
  
  DBG("# Syscall: ");

  switch ( syscall_number )
  {
    case SYS_HALT: 
    {
      DBG("HALT.\n");
      power_off();
      break;
    }
    
    case SYS_EXIT:
    {
      DBG("EXIT. Status code: %d.\n", esp[1]);
      thread_exit();
      break;
    }

    case SYS_CREATE:
    {
      char* name = (char*)esp[1];
      unsigned size = esp[2];
      DBG("CREATE. Filename: %s. Filesize: %u.\n", name, size);
      break;
    }

    case SYS_WRITE:
    {
      int fd = esp[1];
      char* buffer = (char*)esp[2];
      unsigned size = esp[3];
      int32_t ret = size;
      
      DBG("WRITE. Size: %u.", size);
      
      if(fd == STDOUT_FILENO)
      {
        DBG(" Writing to: console. Buffer: \"");
        putbuf(buffer, size);
      }
      else if(fd == STDIN_FILENO)
      {
        DBG(" Writing to wrong buffer!");
        ret = -1;
      }
      else
      {
        DBG(" Writing to: filedescriptor %d.", fd);
        //Implementation later
      }
      
      f->eax = ret;
      DBG("\" Return value: %d.\n", ret);
      break;
    }

    case SYS_READ:
    {
      int fd = esp[1];
      char* buffer = (char*)esp[2];
      unsigned size = esp[3];
      int32_t ret = size;
      
      DBG("READ. Size: %u.", size);
      
      if(fd == STDIN_FILENO)
      {
        DBG(" Reading from: keyboard. Char: \"");
        
        char input;
        char output;
        for(unsigned  i = 0; i < size; i++)
        {
          input = input_getc();
          if(input == '\r')
          {
            output = '\n';
          }
          else
          {
            output = input;
          }
          putbuf(&output, 1);
          buffer[i] = output;
        }

      }
      else if(fd == STDOUT_FILENO)
      {
        DBG(" Reading to wrong buffer!");
        ret = -1;
      }
      else
      {
        DBG(" Reading from: filedescriptor %d.", fd);
        //implementation later
      }

      f->eax = ret;
      DBG("\" Return: %d.\n", f->eax);
      break;
    }
    
    default:
    {
      printf ("Executed an unknown system call!\n");

      printf ("Stack top + 0: %d\n", esp[0]);
      printf ("Stack top + 1: %d\n", esp[1]);

      thread_exit ();
    }
  }
}
