#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <curl/curl.h>
#include <math.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "lab_png.h"
#include "zutil.c"
#include "crc.c"

#define IMG_URL "http://ece252-1.uwaterloo.ca:2520/image?img=1"
#define DUM_URL "https://example.com/"
#define ECE252_HEADER "X-Ece252-Fragment: "
#define BUF_SIZE 1048576  /* 1024*1024 = 1M */
#define BUF_INC  524288   /* 1024*512  = 0.5M */
#define png_position 0
#define IHDR_position 8
#define IDAT_position 33
#define data_position 41
#define max(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

typedef struct recv_buf2 {
    char *buf;       /* memory to hold a copy of received data */
    size_t size;     /* size of valid data in buf in bytes*/
    size_t max_size; /* max capacity of buf in bytes*/
    int seq;         /* >=0 sequence number extracted from http header */
                     /* <0 indicates an invalid seq number */
} RECV_BUF;

struct thread_args              /* thread input parameters struct */
{
    char url[256];
};

size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata);
size_t write_cb_curl3(char *p_recv, size_t size, size_t nmemb, void *p_userdata);
int recv_buf_init(RECV_BUF *ptr, size_t max_size);
int recv_buf_cleanup(RECV_BUF *ptr);
int write_file(const char *path, const void *in, size_t len);
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

RECV_BUF *img;  
int count=0;

void *do_work(void *arg)
{
    CURL *curl_handle;
    CURLcode res;
    RECV_BUF recv_buf;
    img = malloc(sizeof(RECV_BUF)*50);
    struct thread_args *p_in = arg;
    int cnt = 0;
    while(cnt<50){
      img[cnt].seq=-1;
      cnt++;
    }
      
    recv_buf_init(&recv_buf, BUF_SIZE);
    curl_global_init(CURL_GLOBAL_DEFAULT);

    /* init a curl session */
    curl_handle = curl_easy_init();

    if (curl_handle == NULL) {
        fprintf(stderr, "curl_easy_init: returned NULL\n");
        //return 1;
    }

    /* specify URL to get */
    curl_easy_setopt(curl_handle, CURLOPT_URL, p_in->url);

    /* register write call back function to process received data */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_cb_curl3); 
    /* user defined data structure passed to the call back function */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&recv_buf);

    /* register header call back function to process received header data */
    curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_cb_curl); 
    /* user defined data structure passed to the call back function */
    curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, (void *)&recv_buf);

    /* some servers requires a user-agent field */
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    while(count < 50){
      /* get it! */
      res = curl_easy_perform(curl_handle);
      if( res != CURLE_OK) {
          break;
      } 
      if(img[recv_buf.seq].seq == -1){
        img[recv_buf.seq].buf = malloc(sizeof(char)*recv_buf.size);
        memcpy(img[recv_buf.seq].buf, recv_buf.buf,recv_buf.size);
        img[recv_buf.seq].size = recv_buf.size;
        img[recv_buf.seq].max_size = recv_buf.max_size;
        img[recv_buf.seq].seq = recv_buf.seq;
        count++;
      
    }
    
    recv_buf_cleanup(&recv_buf);
    recv_buf_init(&recv_buf, BUF_SIZE);
    }
    /* cleaning up */
    recv_buf_cleanup(&recv_buf);
    curl_easy_cleanup(curl_handle);
    curl_global_cleanup();
    return NULL;
}

size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata)
{
    int realsize = size * nmemb;
    RECV_BUF *p = userdata;
    
    if (realsize > strlen(ECE252_HEADER) &&
    strncmp(p_recv, ECE252_HEADER, strlen(ECE252_HEADER)) == 0) {

        /* extract img sequence number */
	    p->seq = atoi(p_recv + strlen(ECE252_HEADER));

    }
    return realsize;
}

size_t write_cb_curl3(char *p_recv, size_t size, size_t nmemb, void *p_userdata)
{
    size_t realsize = size * nmemb;
    RECV_BUF *p = (RECV_BUF *)p_userdata;
 
    if (p->size + realsize + 1 > p->max_size) {/* hope this rarely happens */ 
        /* received data is not 0 terminated, add one byte for terminating 0 */
        size_t new_size = p->max_size + max(BUF_INC, realsize + 1);   
        char *q = realloc(p->buf, new_size);
        if (q == NULL) {
            perror("realloc"); /* out of memory */
            return -1;
        }
        p->buf = q;
        p->max_size = new_size;
    }

    memcpy(p->buf + p->size, p_recv, realsize); /*copy data from libcurl*/
    p->size += realsize;
    p->buf[p->size] = 0;

    return realsize;
}


int recv_buf_init(RECV_BUF *ptr, size_t max_size)
{
    void *p = NULL;
    
    if (ptr == NULL) {
        return 1;
    }

    p = malloc(max_size);
    if (p == NULL) {
	    return 2;
    }
    
    ptr->buf = p;
    ptr->size = 0;
    ptr->max_size = max_size;
    ptr->seq = -1;              /* valid seq should be non-negative */
    return 0;
}

int recv_buf_cleanup(RECV_BUF *ptr)
{
    if (ptr == NULL) {
	return 1;
    }
    
    free(ptr->buf);
    ptr->size = 0;
    ptr->max_size = 0;
    return 0;
}

int write_file(const char *path, const void *in, size_t len)
{
    FILE *fp = NULL;

    if (path == NULL) {
        fprintf(stderr, "write_file: file name is null!\n");
        return -1;
    }

    if (in == NULL) {
        fprintf(stderr, "write_file: input data is null!\n");
        return -1;
    }

    fp = fopen(path, "wb");
    if (fp == NULL) {
        perror("fopen");
        return -2;
    }

    if (fwrite(in, 1, len, fp) != len) {
        fprintf(stderr, "write_file: imcomplete write!\n");
        return -3; 
    }
    return fclose(fp);
}


int main( int argc, char** argv ) 
{
  
    int c;
    int num_threads=1;
    int num_picture =1;
    char *str = "option requires an argument";
    
    while ((c = getopt (argc, argv, "t:n:")) != -1) {
        switch (c) {
          case 't':
  	      num_threads = strtoul(optarg, NULL, 10);
    	    if (num_threads <= 0) {
                fprintf(stderr, "%s: %s > 0 -- 't'\n", argv[0], str);
                return -1;
         }
         break;
         case 'n':
            num_picture = strtoul(optarg, NULL, 10);
            if (num_picture <= 0 || num_picture > 3) {
                fprintf(stderr, "%s: %s 1, 2, or 3 -- 'n'\n", argv[0], str);
                return -1;
            }
            break;
        default:
            return -1;
        }

    }
 
    if(num_threads==1) //single thread
    {
      CURL *curl_handle;
      CURLcode res;
      char url[256];
      RECV_BUF recv_buf;
      img = malloc(sizeof(RECV_BUF)*50);
      memset(img, 0, 50*sizeof(RECV_BUF));
      int cnt = 0;
      while(cnt<50){
        img[cnt].seq=-1;
        cnt++;
        }
       
      recv_buf_init(&recv_buf, BUF_SIZE);
      
      if (argc == 1) {
          strcpy(url, IMG_URL); 
      } else{
          sprintf(url,"http://ece252-%d.uwaterloo.ca:2520/image?img=%d",rand()%3+1,num_picture);
      }
      curl_global_init(CURL_GLOBAL_DEFAULT);
  
      /* init a curl session */
      curl_handle = curl_easy_init();
  
      if (curl_handle == NULL) {
          fprintf(stderr, "curl_easy_init: returned NULL\n");
          return 1;
      }
  
      /* specify URL to get */
      curl_easy_setopt(curl_handle, CURLOPT_URL, url);
  
      /* register write call back function to process received data */
      curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_cb_curl3); 
      /* user defined data structure passed to the call back function */
      curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&recv_buf);
  
      /* register header call back function to process received header data */
      curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_cb_curl); 
      /* user defined data structure passed to the call back function */
      curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, (void *)&recv_buf);
  
      /* some servers requires a user-agent field */
      curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
      cnt=0;
      while(cnt < 50){
        /* get it! */
        res = curl_easy_perform(curl_handle);
        if( res != CURLE_OK) {
            break;
        } 
        if(img[recv_buf.seq].seq == -1){
          img[recv_buf.seq].buf = malloc(sizeof(char)*recv_buf.size);
          memcpy(img[recv_buf.seq].buf, recv_buf.buf,recv_buf.size);
          img[recv_buf.seq].size = recv_buf.size;
          img[recv_buf.seq].max_size = recv_buf.max_size;
          img[recv_buf.seq].seq = recv_buf.seq;
          cnt++;
        
      }
      recv_buf_cleanup(&recv_buf);
      recv_buf_init(&recv_buf, BUF_SIZE);
    }
  }
  else //multiple threads
  { 
      pthread_t *p_tids = malloc(sizeof(pthread_t) * num_threads);
      struct thread_args in_params[num_threads];
      
      for(int j=0;j<num_threads;j++)
      {
        sprintf(in_params[j].url,"http://ece252-%d.uwaterloo.ca:2520/image?img=%d",rand()%3+1,num_picture);
        pthread_create(p_tids + j, NULL, do_work, in_params + j);
      }
      for(int j=0;j<num_threads;j++)
      {
        pthread_join(p_tids[j], NULL);
      }
       free(p_tids);
  }
  
   //write the png file 
   unsigned long int raw_size = 300 * (400* 4 + 1);
   unsigned char IDAT_data[raw_size];  
    memset(IDAT_data,'\0', sizeof(unsigned char)*(raw_size));
    
    U64 inf_cnt=0;
    U64 def_cnt=0;

  for(int i = 0; i < 50; ++i){
    unsigned char IDAT_size[4];
    memcpy(&IDAT_size, img[i].buf+IDAT_position,4);
    U64 IDAT_length = decimal(IDAT_size,4);
    U8 IDAT_data_len[IDAT_length];
    memcpy(&IDAT_data_len, img[i].buf+data_position,IDAT_length);
    def_cnt=def_cnt+IDAT_length;

    U64 size = 6 * (400 * 4 + 1);
    U8 dest[size];
    
    int inf = mem_inf(dest, &size, IDAT_data_len, IDAT_length);
  
    for (int k=0; k<size;k++)
    {
      IDAT_data[inf_cnt+k]=dest[k];
    }
    
    inf_cnt=inf_cnt+size;
    
  }

  unsigned char IDAT_data_out[def_cnt];
  int def=mem_def(IDAT_data_out, &def_cnt, IDAT_data,inf_cnt,Z_BEST_COMPRESSION);
  
  char* output=malloc(sizeof(char)*BUF_SIZE);
  memset(output, '\0',sizeof(char)*BUF_SIZE);
  
  unsigned char png[8]={0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
  memcpy(output, &png,8);
  
  /* IHDR */
  
  memcpy(output+IHDR_position, img[0].buf+IHDR_position,8);
  U32 IHDR_width=(U32)htonl(400);
  U32 IHDR_height=(U32)htonl(300);

  memcpy(output+IHDR_position+8, &IHDR_width, 4);
  memcpy(output+IHDR_position+12, &IHDR_height, 4);
  
  memcpy(output+IHDR_position+16, img[0].buf+IHDR_position+16, 5);
  unsigned char crc1[17];
  memcpy(&crc1,output+IHDR_position+4,17);
  unsigned long crc_IHDR = crc(crc1, 17);
  U32 crc_IHDR_32=(U32)htonl(crc_IHDR);
  memcpy(output+IHDR_position+21,&crc_IHDR_32,4);
  
  /* IDAT */
  U32 def_cnt_32=(U32)htonl(def_cnt);
  memcpy(output+IDAT_position, &def_cnt_32, 4);
  memcpy(output+IDAT_position+4, img[0].buf+IDAT_position+4,4);
  memcpy(output+data_position, &IDAT_data_out, def_cnt);
  
  unsigned char crc2[def_cnt+4];
  memcpy(&crc2,output+IDAT_position+4,def_cnt+4);
  unsigned long crc_IDAT = crc(crc2, def_cnt+4);
  U32 crc_IDAT_32=(U32)htonl(crc_IDAT);
  memcpy(output+data_position+def_cnt,&crc_IDAT_32,4);
  
  /* IEND */
  unsigned char input_IDATlength[4];
  memcpy(&input_IDATlength, img[0].buf+IDAT_position,4);
  U32 input_IDAT=decimal(input_IDATlength,4);
  memcpy(output+data_position+def_cnt+4, img[0].buf+data_position+input_IDAT+4,8);
  unsigned char crc3[4];
  memcpy(&crc3, output+data_position+def_cnt+8, 4); 
  unsigned long crc_IEND = crc(crc3, 4);
  U32 crc_IEND_32=(U32)htonl(crc_IEND);
  memcpy(output+data_position+def_cnt+12, &crc_IEND_32, 4);
  
  char fname[256];
  sprintf(fname, "./all.png");
  write_file(fname, output, def_cnt+33+12+12);

    /* cleaning up */
  
  free(output);
  for (int i=0; i<50; i++) {
    free(img[i].buf);
  }
  free(img);
  return 0;
}
 //gcc -std=c99 -D_GNU_SOURCE -Wall -O2 -o paster paster.c -lcurl -lm -lz -lpthread