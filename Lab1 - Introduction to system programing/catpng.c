#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <arpa/inet.h>
#include "lab_png.h"
#include "zutil.c"
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

/*get width of the png file*/
int get_width(FILE* fp)
{
  unsigned char width[4];

  fseek(fp, 16, SEEK_SET);
  fread(width,4,1,fp);
  int dec_width = decimal(width,4);
  
  return dec_width;
}

/*get height of the png file*/
int get_height(FILE* fp)
{
  unsigned char height[4];

  fseek(fp, 20, SEEK_SET);
  fread(height,4,1,fp);
  int dec_height = decimal(height,4);
  
  return dec_height;
}

int main(int argc, char* argv[]){
  if(argc < 2){
    printf("Warning: not enough arguments\n");
    return -1;
  }

  int width = 0;
  int new_height = 0;

  /*get width and height*/
  for(int i = 1; i < argc; ++i){
    FILE *fp;
    fp = fopen(argv[i], "rb");

    width = get_width(fp);
    new_height += get_height(fp);

    fclose(fp);
  }
    
  unsigned long int raw_size = new_height * (width * 4 + 1);
  unsigned char IDAT_data[raw_size];
  memset(IDAT_data,'\0', sizeof(unsigned char)*(raw_size));

  U64 inf_cnt=0;
  U64 def_cnt=0;

  for(int i = 1; i < argc; ++i){
    FILE *fp;
    fp = fopen(argv[i], "rb");
    unsigned char IDAT_size[4];
    
    fseek(fp,33,SEEK_SET);
    fread(IDAT_size,4,1,fp);
    
    U64 IDAT_length = decimal(IDAT_size,4);
      
    U8 IDAT_data_len[IDAT_length];
    fseek(fp,41,SEEK_SET);
    fread(IDAT_data_len,IDAT_length,1,fp);

    def_cnt=def_cnt+IDAT_length;

    int dest_width = get_width(fp);
    int dest_height = get_height(fp);

    /* check if width is euqal */
    if(width != dest_width){
      printf("The width of PNG files is not equal\n");
      fclose(fp);
      return -1;
    }

    U64 size = dest_height * (dest_width * 4 + 1);
    U8 dest[size];
    
    int inf = mem_inf(dest, &size, IDAT_data_len, IDAT_length);
    
    for (int k=0; k<size;k++)
    {
      IDAT_data[inf_cnt+k]=dest[k];
    }
    
    inf_cnt=inf_cnt+size;
    fclose(fp);
  }

  unsigned char IDAT_data_out[def_cnt];
  int def=mem_def(IDAT_data_out, &def_cnt, IDAT_data,inf_cnt,Z_BEST_COMPRESSION);

  FILE *input;
  input=fopen(argv[1],"rb");                 

  FILE* output=fopen("all.png","w+");
  
  //write data type
  unsigned char png[8]={0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
  fwrite(png,8,1,output);

  //write IHDR
  unsigned char IHDR_sizetype[8];
  fseek(input,8,SEEK_SET);
  fread(IHDR_sizetype,8,1,input);
  fwrite(IHDR_sizetype,8,1,output);

  //write width and height
  U32 IHDR_width=(U32)htonl(width);
  fwrite(&IHDR_width,4,1,output);

  U32 IHDR_height=(U32)htonl(new_height);
  fwrite(&IHDR_height,4,1,output);

  //write other data
  unsigned char IHDR_data[5];
  fseek(input,24,SEEK_SET);
  fread(IHDR_data,5,1,input);
  fwrite(IHDR_data,5,1,output);

  //calculate IHDR crc
  unsigned char crc1[17];
  fseek(output,12,SEEK_SET);
  fread(crc1,17,1,output);
  unsigned long crc_IHDR = crc(crc1, 17);
  U32 crc_IHDR_32=(U32)htonl(crc_IHDR);
  fwrite(&crc_IHDR_32,4,1,output);

  //write IDAT size
  U32 def_cnt_32=(U32)htonl(def_cnt);
  fwrite(&def_cnt_32,4,1,output);

  //write IDAT type
  unsigned char IDAT_type[4];
  fseek(input,37,SEEK_SET);
  fread(IDAT_type,4,1,input);
  fwrite(IDAT_type,4,1,output);

  //write IDAT data
  fwrite(&IDAT_data_out,1,def_cnt,output);

  //write IDAT crc 
  unsigned char crc2[def_cnt+4];
  fseek(output,37,SEEK_SET);
  fread(crc2,1,def_cnt+4,output);
  unsigned long crc_IDAT = crc(crc2, def_cnt+4);
  U32 crc_IDAT_32=(U32)htonl(crc_IDAT);
  fwrite(&crc_IDAT_32,4,1,output);

  //write IEND typelength
  unsigned char input_IDATlength[4];
  fseek(input,33,SEEK_SET);
  fread(input_IDATlength,1,4,input);
  U32 input_IDAT=decimal(input_IDATlength,4);

  unsigned char IEND_typelength[8];
  fseek(input,45+input_IDAT,SEEK_SET);
  fread(IEND_typelength,8,1,input);
  fwrite(IEND_typelength,8,1,output);

  //write IEND crc
  unsigned char crc3[4];
  fseek(output,49+def_cnt,SEEK_SET);
  fread(crc3,1,4,output);
  unsigned long crc_IEND = crc(crc3, 4);
  U32 crc_IEND_32=(U32)htonl(crc_IEND);
  fwrite(&crc_IEND_32,4,1,output);

  fclose(input);
  fclose(output);

  return 0;
}
//gcc -std=c99 -D_GNU_SOURCE -Wall -O2 -o catpng catpng.c -lm -lz
