#ifndef LIST_H
#define LIST_H

#include <unistd.h>

typedef struct list {
  struct list *prev;
  struct list *next;
  size_t       elem_size;
  void        *content;
} List;

List *newList(size_t elem_size);
void  deleteList(List **list);
void  push(List *list, void *elem);
void *pop(List *list);
void  insertAt(List *list, int index, void *elem);
void *removeAt(List *list, int index);
int   indexOf(List *list, void *);
void *at(List *list, int index);
int   sizeOf(List *list);

#endif