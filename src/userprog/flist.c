#include <stddef.h>
#include "flist.h"
#include "filesys/file.h"

void flist_init(struct flist* m)
{
  for(int i = 0; i < FLIST_SIZE; i++)
  {
    m->content[i] = NULL;
  }
}

key_t flist_insert(struct flist* m, value_t v)
{

  for(int i = 0; i < FLIST_SIZE; i++)
  {
    if(m->content[i] == NULL)
    {
      m->content[i] = v;
      return i;
    }
  }
  return -1;
}

value_t flist_find(struct flist* m, key_t k)
{
  //k = k-2;
  if(k < 0 || k >= FLIST_SIZE)
  { 
    return NULL;
  }
  else
  {
    return m->content[k];
  }
}

value_t flist_remove(struct flist* m, key_t k)
{
  //k = k-2;
  if(flist_find(m, k) == NULL)
  {
    return NULL;
  }
  else
  {   
    value_t ret = m->content[k];
    m->content[k] = NULL;
    return ret;
  }
}

void flist_cleanup(struct flist* m)
{
  for(int i = 0; i < FLIST_SIZE; i++)
  {
    struct file* file_ptr = flist_find(m, i);
    if(file_ptr)
    {
      file_close(flist_find(m, i));
      flist_remove(m, i);
    }
  }
}
