#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <math.h>
#include <limits.h>
#include "lab_png.h"
#include "crc.c"
#include "linked_list.c"

/* check if a file is png */
int check_png(char* png){
  char buffer[10];
  FILE *fp;
  fp=fopen(png,"rb");                 

  size_t bytes_read = 0;
  bytes_read = fread(buffer, sizeof(unsigned char), 9, fp);
  buffer[bytes_read] = '\0';
  unsigned char png_format[8]={0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
  if(memcmp(png_format,buffer,8))
  {
    fclose(fp);
    return 0;
  }
  else{
    fclose(fp);
    return 1;
  }
}

int check_dir(char* name, node_t* head){

  static int result=0;
  DIR* dir=opendir(name);
  
  struct dirent *p_dirent;
  
  if(dir == NULL){
    printf("Not a directory\n");
    closedir(dir);
    return -1;
  }
  
  while((p_dirent = readdir(dir)) != NULL){

    char* str_path= p_dirent->d_name;
    
    struct stat buf;
    if (str_path == NULL){
      printf("Null pointer found!"); 
      exit(3);
    } 
    else{
      if(strcmp(".", str_path)&&strcmp("..", str_path)) {
        char* path=malloc(100000* sizeof(char));;
        memset(path, '\0', sizeof(char)*100000);
        sprintf(path,"%s/%s",name,p_dirent->d_name);
      
        if(lstat(path,&buf) < 0) 
        {
          return -1;
        }
        /* regular file */
        if(S_ISREG(buf.st_mode)){
          if(check_png(path)){
            push(&head,path);
            result++;
          }
        }
        /* directory */
        else if(S_ISDIR(buf.st_mode)){
          check_dir(path, head);
        }
        /* others */
        else{}
      }
    }
  }
  closedir(dir);
  
  /* print path */
  if(head == NULL){}
  else{
    print_node(head);
  }
  
  return result;
}

int main(int argc, char *argv[]) 
{
  if (argc!=2)
	{
    printf("Usage:two arguments needed\n");
    return EXIT_FAILURE;
	}
  
  node_t* head = (node_t*)malloc(sizeof(node_t));
  head = NULL;
  
  int result = check_dir(argv[1], head);

  if(result==0){
    printf("findpng: No PNG file found\n");
  }
}
//gcc -std=c99 -D_GNU_SOURCE -Wall -O2 -o findpng findpng.c