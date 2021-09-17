#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct node{
  char* filename;
  struct node* next;
}node_t;

void push(node_t** head, char* file){
  node_t* temp = (node_t*)malloc(sizeof(struct node));
  temp->filename = malloc(sizeof(char));
  temp->filename = file;
  temp->next = (*head);
  
  *head = temp;
}

void print_node(node_t* head){
  node_t* curr = (node_t*)malloc(sizeof(struct node));
  curr = head;
  while(curr != NULL){
    printf("%s\n", curr->filename);
    curr = curr->next; 
  }
}