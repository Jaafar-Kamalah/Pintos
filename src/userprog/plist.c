#include <stddef.h>
#include <stdio.h>
#include "plist.h"
#include <stdlib.h>


struct plist process_list; //define global plist

void plist_init(void)
{
  for(int i = 0; i < PLIST_SIZE; i++)
  {
    process_list.content[i] = NULL;
  }
}

int plist_insert(struct process* v)
{

  for(int i = 0; i < PLIST_SIZE; i++)
  {
    if(process_list.content[i] == NULL)
    {
      process_list.content[i] = v;
      return i;
    }
  }
  return -1;
}

struct process* plist_find(int k)
{
  if(k < 0 || k >= PLIST_SIZE)
  {
    return NULL;
  }
  else
  {
    return process_list.content[k];
  }
}

struct process* plist_remove(int k)
{
  if(plist_find(k) == NULL)
  {
    return NULL;
  }
  else
  {
    struct process* ret = process_list.content[k];
    process_list.content[k] = NULL;
    return ret;
  }
}

void plist_print(void)
{
  printf("////////////////////PROCESS_LIST////////////////////\n");
  printf("%3s %20s %9s %14s %13s %14s\n", "PID", "Name", "State",
         "Exit Status", "Parent PID", "Parent State");

  for(int i = 0; i < PLIST_SIZE; i++)
    {
      if(process_list.content[i] != NULL)
        {
          printf("%3d %20s %9s %14d %13d %14s\n", i,
                 process_list.content[i]->name,
                 process_list.content[i]->dead ? "dead" : "alive",
                 process_list.content[i]->exit_status,
                 process_list.content[i]->parent_pid,
                 process_list.content[i]->parent_dead ? "dead" : "alive");
        }
    }
  printf("////////////////////////////////////////////////////\n");

}
