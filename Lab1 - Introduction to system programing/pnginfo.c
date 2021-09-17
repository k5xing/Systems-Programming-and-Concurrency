#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include "lab_png.h"
#include "crc.c"

/*convert hex to decimal*/
U32 decimal(unsigned char hex[], int length)
{
  int exp = 0;
  U32 dec = 0; 
  while(length > 0){
    dec = dec + hex[length - 1] * pow(256,exp);
    --length;
    ++exp;
  }
  
  return dec; 
}

/*convert decimal to hex*/
char* hex(U32 decimal)
{
  char* hex_val = malloc(sizeof(char)*100);
  memset(hex_val, '0', sizeof(char)*100);
  
  U32 quo = decimal;
  U32 remaindar = 0;
  
  int j = 0;
  while(quo != 0)
  {
    remaindar = quo % 16;

    if(remaindar < 10)
    {
      hex_val[j++] = 48 + remaindar;
    }
    else
    {
      hex_val[j++] = 55 + remaindar +32;
    }
    quo = quo / 16;
  }

  return hex_val;
}

/*print hex value*/
void print_hex(char* input)
{
  int len = 0;
   while(input[len] != '0')
   {
  
     ++len;
   }
   len = len - 1;
   while(len >= 0)
   {
     printf("%c", input[len]);
     --len;
   }
  
}

int main(int argc, char *argv[]) 
{
  unsigned char buffer[1000000];
  if (argc!=2)
	{
		printf("Usage: two arguments needed.\n");
    return -1;
	}
  FILE *fp;
  fp=fopen(argv[1],"rb");                 

  size_t bytes_read = 0;
  bytes_read = fread(buffer, sizeof(unsigned char), 999999, fp);
  buffer[bytes_read] = '\0';
  unsigned char png[8]={0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};

  if(memcmp(png,buffer,8))
  {
    printf("%s: Not a PNG file\n",argv[1]);
  }
  else
  {
    simple_PNG_p png  = malloc(sizeof(simple_PNG_p));
    chunk_p p_IHDR = malloc(sizeof(chunk_p));
    
    /*IHDR */
    unsigned char buffer_IHDR[26];
    memcpy(buffer_IHDR,buffer+8,25);
    buffer[25]='\0';
    
    p_IHDR->length=13;
    int cnt=0;
    while(cnt<3)
    {
      p_IHDR->type[cnt]=buffer_IHDR[cnt+8];
      cnt++;
    }
    
    p_IHDR->p_data=&buffer_IHDR[8];
    unsigned char IHDR_crc[5];
    memcpy(IHDR_crc,buffer_IHDR+21,4);
    IHDR_crc[4]='\0';
    p_IHDR->crc = decimal(IHDR_crc,4);
    
    data_IHDR_p data = malloc(sizeof(data_IHDR_p));
    
    unsigned char width[5];
    memcpy(width, buffer_IHDR+8, 4);
    width[4] = '\0';
    data->width = decimal(width,4);
  
    unsigned char height[5];
    memcpy(height, buffer_IHDR+12, 4);
    height[4] = '\0';
    data->height = decimal(height,4);
    data->bit_depth=buffer_IHDR[16]; 
    data->color_type=buffer_IHDR[17];  
    data->compression=buffer_IHDR[18];  
    data->filter=buffer_IHDR[19];      
    data->interlace=buffer_IHDR[20];  
    
    /* IHDR crc */
    unsigned char crc1[18];
    memcpy(crc1, buffer_IHDR+4, 17);
    crc1[17] = '\0';
    unsigned long crc_IHDR = crc(crc1, 17);
    
    /* IDAT */
    chunk_p p_IDAT = malloc(sizeof(chunk_p));
    unsigned char buffer_IDAT_LENGTH[5];
    memcpy(buffer_IDAT_LENGTH,buffer+33,4);
    buffer_IDAT_LENGTH[4]='\0';
  
    p_IDAT->length=decimal(buffer_IDAT_LENGTH,4);
    unsigned char buffer_IDAT[p_IDAT->length+13];
    memcpy(buffer_IDAT,buffer+33,p_IDAT->length+12);
    buffer_IDAT[p_IDAT->length+12]='\0';
    
    cnt=0;
    while(cnt<3)
    {
      p_IDAT->type[cnt]=buffer_IDAT[cnt+8];
      cnt++;
    }
    
    p_IDAT->p_data=&buffer_IDAT[8];
    unsigned char IDAT_crc[5];
    memcpy(IDAT_crc,buffer_IDAT+p_IDAT->length+8,4);
    IDAT_crc[4]='\0';
    p_IDAT->crc=decimal(IDAT_crc,4);
    
    /* IDAT crc */
    unsigned char crc2[p_IDAT->length+5];
    memcpy(crc2, buffer_IDAT+4, p_IDAT->length+4);
    crc2[p_IDAT->length+4] = '\0';
    unsigned long crc_IDAT = crc(crc2, p_IDAT->length+4);
    
    /* IEND */
    chunk_p p_IEND = malloc(sizeof(chunk_p));
    unsigned char buffer_IEND_LENGTH[5];
    memcpy(buffer_IEND_LENGTH,buffer+33+12+p_IDAT->length,4);
    buffer_IEND_LENGTH[4]='\0';
  
    p_IEND->length=decimal(buffer_IEND_LENGTH,4);
    unsigned char buffer_IEND[p_IEND->length+13];
    memcpy(buffer_IEND,buffer+33+12+p_IDAT->length,p_IEND->length+12);
    buffer_IDAT[p_IEND->length+12]='\0';
    
    cnt=0;
    while(cnt<3)
    {
      p_IEND->type[cnt]=buffer_IEND[cnt+8];
      cnt++;
    }
    
    p_IEND->p_data=&buffer_IEND[8];
    unsigned char IEND_crc[5];
    memcpy(IEND_crc,buffer_IEND+p_IEND->length+8,4);
    IEND_crc[4]='\0';
    p_IEND->crc=decimal(IEND_crc,4);
    
    unsigned char crc3[p_IEND->length+5];
    memcpy(crc3, buffer_IEND+4, p_IEND->length+4);
    crc3[p_IEND->length+4] = '\0';
    unsigned long crc_IEND = crc(crc3, p_IEND->length+4);

    /* check crc */
    if(crc_IHDR != p_IHDR->crc)
    {
      char* ihdr = hex(p_IHDR->crc);
      char* ihdr_compute = hex(crc_IHDR);
      printf("IHDR chunk CRC error: computed ");
      print_hex(ihdr_compute);
      printf(", expected ");
      print_hex(ihdr);
      printf("\n");
    }
    else if(crc_IDAT != p_IDAT->crc)
    {
      char* idat = hex(p_IDAT->crc);
      char* idat_compute = hex(crc_IDAT);
      printf("IDAT chunk CRC error: computed ");
      print_hex(idat_compute);
      printf(", expected ");
      print_hex(idat);
      printf("\n");
    }
    else if(crc_IEND != p_IEND->crc)
    {
      char* iend = hex(p_IEND->crc);
      char* iend_compute = hex(crc_IEND);
      printf("IEND chunk CRC error: computed ");
      print_hex(iend_compute);
      printf(", expected ");
      print_hex(iend);
      printf("\n");
    }
    else
    {
      printf("%s: %d x %d\n",argv[1], data->width, data->height);
    }
   
    /*PNG */
    png->p_IHDR=p_IHDR;
    png->p_IDAT=p_IDAT;
    png->p_IEND=p_IEND;
    
    free(png);
    free(p_IHDR);
    free(p_IDAT);
    free(p_IEND);
    free(data);
  }
  
  fclose(fp);
}

//gcc -std=c99 -D_GNU_SOURCE -Wall -O2 -o pnginfo pnginfo.c -lm
