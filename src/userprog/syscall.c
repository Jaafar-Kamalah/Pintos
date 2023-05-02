#include <stdio.h>
#include <syscall-nr.h>
#include "userprog/syscall.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/timer.h"
#include "plist.h"
#include "flist.h"

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
#define DBG(format, ...) //printf(format, ##__VA_ARGS__)

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
    int exit_status = esp[1];
    DBG("EXIT. Status code: %d.\n", exit_status);
    process_exit(exit_status);
    thread_exit();
    break;
  }

  case SYS_EXEC:
  {
    char* file = (char*)esp[1];
    int32_t ret;
        
    DBG("EXEC. Filename: %s.\n", file);
    ret = process_execute(file);
    f->eax = ret;
    DBG("Exec return value: %d.\n", f->eax);  
    break;
  }

  case SYS_WAIT:
  {
    int child = esp[1];
    int32_t ret;
    
    DBG("WAIT. Child PID: %d.\n", child);
    ret = process_wait(child);
    f->eax = ret;
    DBG("Wait return value: %d.\n", f->eax);      
    break;
  }

  case SYS_CREATE:
  {
    char* name = (char*)esp[1];
    unsigned size = esp[2];
    int32_t ret;
     
    DBG("CREATE. Filename: %s. Filesize: %u.", name, size);
    if(filesys_create(name, size))
    {
      DBG(" Creation successful!");
      ret = true;
    }
    else
    {
      DBG(" Creation failed!\n");
      ret = false;
    }
      
    f->eax = ret;
    DBG(" Return value: %d.\n", f->eax);      
    break;
  }

  case SYS_REMOVE:
  {
    char* name = (char*)esp[1];
    int32_t ret;
      
    DBG("REMOVE. Filename: %s.", name);
    if(filesys_remove(name))
    {
      DBG(" Remove successful!");
      ret = true;
    }
    else
    {
      DBG(" Remove failed!");
      ret = false;
    }
      
    f->eax = ret;
    DBG(" Return value: %d.\n", f->eax);      
    break;
  }

  case SYS_OPEN:
  {
    char* name = (char*)esp[1];
    int32_t ret;
    //NULL if name does not exist
    struct file* file_ptr = filesys_open(name);
      
    DBG("OPEN. Filename: %s.", name);
    if(file_ptr)
    {
      //Inserts new file in open_files flist and returns corresponding index.
      //Since index 0 and 1 for fd are reserved we want to start at 2.
      ret = flist_insert(&thread_current()->open_files, file_ptr) + 2;
      if(ret == -1)
      {
        //Not enough space in flist, close file
        filesys_close(file_ptr);
      }
    }
    else
    {
      DBG(" File does not exist!");
      ret = -1;
    }
      
    f->eax = ret;
    DBG(" Return value: %d.\n", f->eax);      
    break;
  }
    
  case SYS_CLOSE:
  {
    int fd = esp[1];
    //NULL if fd-2 is not open.
    struct file* file_ptr = flist_find(&thread_current()->open_files, fd-2);
  
    DBG("CLOSE. Filedescriptor: %d.\n", fd);
    if(file_ptr)
    {
      file_close(flist_find(&thread_current()->open_files, fd-2));
      flist_remove(&thread_current()->open_files, fd-2);
    }
    else
    {
      DBG(" File is not open!");
    }
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
      DBG("\"");
    }
    else if(fd == STDIN_FILENO)
    {
      DBG(" Writing to wrong buffer!");
      ret = -1;
    }
    else
    {
      DBG(" Writing to: filedescriptor %d.", fd);
      //NULL if fd-2 is not open.
      struct file* file_ptr = flist_find(&thread_current()->open_files, fd-2);
      if(file_ptr)
      {
        ret = file_write(file_ptr, buffer, size);
      }
      else
      {
        DBG(" File not open!");
        ret = -1;
      }
    }
      
    f->eax = ret;
    DBG(" Return value: %d.\n", f->eax);
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
      char input;
      char output;
      DBG(" Reading from: keyboard. Char: \"");
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
      DBG("\"");
    }
    else if(fd == STDOUT_FILENO)
    {
      DBG(" Reading to wrong buffer!");
      ret = -1;
    }
    else
    {
      DBG(" Reading from: filedescriptor %d.", fd);
      //NULL if fd-2 is not open.
      struct file* file_ptr = flist_find(&thread_current()->open_files, fd-2);
      if(file_ptr)
      {
        ret = file_read(file_ptr, buffer, size);
      }
      else
      {
        DBG(" File not open!"); 
        ret = -1;
      }
    } 

    f->eax = ret;
    DBG(" Return: %d.\n", f->eax);
    break;
  }

  case SYS_FILESIZE:
  {
    int fd = esp[1];
    int32_t ret;
    //NULL if fd-2 is not open.
    struct file* file_ptr = flist_find(&thread_current()->open_files, fd-2);

    DBG("FILESIZE. Filedescriptor: %d.", fd);
    if(file_ptr)
    {
      ret = file_length(file_ptr);
    }
    else
    {
      DBG(" File is not open!");
      ret = -1;
    }

    f->eax = ret;
    DBG("# Return: %d.\n", f->eax);
    break;
  }

  case SYS_SEEK:
  {
    int fd = esp[1];
    unsigned position = esp[2];
    struct file* file_ptr = flist_find(&thread_current()->open_files, fd-2);
      
    DBG("SEEK. Filedescriptor: %d. Position: %u.\n", fd, position);
    if(file_ptr)
    {
      if(position > (unsigned)file_length(file_ptr))
      {
        DBG(" Position is larger than filesize.\n");
        position = file_length(file_ptr);
      }
      file_seek(file_ptr, position);
    }      
    break;
  }

  case SYS_TELL:
  {
    int fd = esp[1];
    int32_t ret;      
    struct file* file_ptr = flist_find(&thread_current()->open_files, fd-2);
      
    DBG("TELL. Filedescriptor: %d.", fd);
    if(file_ptr)
    {
      ret = file_tell(file_ptr);
    }
    else
    {
      ret = -1;
    }
      
    f->eax = ret;
    DBG("# Return: %d.\n", f->eax);
    break;
  }

  case SYS_PLIST:
  {
    DBG("PLIST.\n");
    plist_print();
    break;
  }
    
  case SYS_SLEEP:
  {
    int ms = esp[1];
    DBG("SLEEP. Milliseconds: %d.\n", ms);
    timer_msleep(ms);
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
