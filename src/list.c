#include <list.h>
#include <stdlib.h>
#include <string.h>

List *newList(size_t elem_size) {
  List *l      = malloc(sizeof(List));
  l->prev      = NULL;
  l->next      = NULL;
  l->elem_size = elem_size;
  l->content   = NULL;
  return l;
}

void deleteList(List **list) {
  for (; *list; *list = (*list)->next) {
    free((*list)->content);
    free(*list);
  }
  *list = NULL;
}

void push(List *list, void *elem) {
  for (; list->next; list = list->next);
  List *tmp      = malloc(sizeof(List));
  tmp->prev      = list;
  tmp->next      = NULL;
  tmp->elem_size = list->elem_size;
  tmp->content   = malloc(tmp->elem_size);
  memcpy(tmp->content, elem, tmp->elem_size);
  list->next = tmp;
}

void *pop(List *list) {
  void *content = NULL;
  for (; list->next; list = list->next);
  if (list->prev) {
    list->prev->next = NULL;
    content = list->content;
    free(list);
  }
  return content;
}

void insertAt(List *list, int index, void *elem) {
  for (int i = 0; list->next && i < index; list = list->next, i++);
  List *next = list->next;
  List *tmp = malloc(sizeof(List));
  tmp->prev = list;
  tmp->next = next;
  tmp->elem_size = list->elem_size;
  tmp->content = malloc(tmp->elem_size);
  memcpy(tmp->content, elem, tmp->elem_size);
  list->next = tmp;
  if (next) next->prev = tmp;
}

void *removeAt(List *list, int index) {
  void *content = NULL;
  for (int i = -1; list->next && i < index; list = list->next, i++);
  if (list->content) {
    if (list->prev) list->prev->next = list->next;
    if (list->next) list->next->prev = list->prev;
    content = list->content;
    free(list);
  }
  return content;
}

int indexOf(List *list, void *elem) {
  int i;
  for (i = -1; list->next && list->content != elem; list = list->next, i++);
  if (list->content == elem) return i;
  return -1;
}

void *at(List *list, int index) {
  for (int i = -1; list->next && i < index; list = list->next, i++);
  if (list) return list->content;
  return NULL;
}

int sizeOf(List *list) {
  int i;
  for (i = 0; list->next; list = list->next, i++);
  return i;
}