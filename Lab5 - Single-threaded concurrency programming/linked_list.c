#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct node{
  char* png_url;
  struct node* next;
}node_t;

void push(node_t** head, char* file){
 
  node_t* temp = (node_t*)malloc(sizeof(struct node));
  temp->png_url = malloc(sizeof(char)*1000);
  strcpy(temp->png_url, file);
  temp->next = (*head);
  
  *head = temp;
}

void print_node(node_t* head, const char* filename){
  node_t* curr = (node_t*)malloc(sizeof(struct node));
  curr = head;
  FILE* fp=fopen(filename, "w+");
  char s[256];
  while(curr != NULL){
    sprintf(s, "%s\n", curr->png_url);
    fwrite(s, strlen(s), 1, fp);
    curr = curr->next; 
  }
  fclose(fp);
}

char* pop(node_t** head){
  if(*head == NULL){
    return NULL;
  }
  char* url = malloc(sizeof(char)*10000);
  url=(*head)->png_url;
  node_t* temp = *head;
  *head = (*head)->next;
  free(temp);
  return url;
}

int is_empty(node_t* head){
  if(head == NULL){
    return 1;
  }
  return 0;
}

void free_list(node_t *head){
  node_t* temp = head;
  while(head!=NULL){
    head=head->next;
    free(temp->png_url);
    free(temp);
    temp=head;
  }
}