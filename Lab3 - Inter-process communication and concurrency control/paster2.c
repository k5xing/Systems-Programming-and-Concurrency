#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <math.h>
#include <unistd.h>
#include <curl/curl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/ipc.h>
#include "shm_stack.c"
#include "lab_png.h"
#include "zutil.c"
#include "crc.c"


#define ECE252_HEADER "X-Ece252-Fragment: "
// #define BUF_SIZE 10240  /* 1024*10 = 10K */
#define MAX_SIZE 1048576
#define RAW_SIZE 300 * (400* 4 + 1)
#define png_position 0
#define IHDR_position 8
#define IDAT_position 33
#define data_position 41

size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata);
size_t write_cb_curl(char *p_recv, size_t size, size_t nmemb, void *p_userdata);
int recv_buf_init(RECV_BUF *ptr, size_t max_size);
int recv_buf_cleanup(RECV_BUF *ptr);
int write_file(const char *path, const void *in, size_t len);

// typedef struct inflated{
//     U8* data;
// } INFLATED;

// int init_inflated(INFLATED* inflated)
// {

//    inflated->data = (U8 *) (inflated + sizeof(INFLATED));
    
//     return 0;
// }

/**
 * @brief  cURL header call back function to extract image sequence number from 
 *         http header data. An example header for image part n (assume n = 2) is:
 *         X-Ece252-Fragment: 2
 * @param  char *p_recv: header data delivered by cURL
 * @param  size_t size size of each memb
 * @param  size_t nmemb number of memb
 * @param  void *userdata user defined data structurea
 * @return size of header data received.
 * @details this routine will be invoked multiple times by the libcurl until the full
 * header data are received.  we are only interested in the ECE252_HEADER line 
 * received so that we can extract the image sequence number from it. This
 * explains the if block in the code.
 */
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


/**
 * @brief write callback function to save a copy of received data in RAM.
 *        The received libcurl data are pointed by p_recv, 
 *        which is provided by libcurl and is not user allocated memory.
 *        The user allocated memory is at p_userdata. One needs to
 *        cast it to the proper struct to make good use of it.
 *        This function maybe invoked more than once by one invokation of
 *        curl_easy_perform().
 */

size_t write_cb_curl(char *p_recv, size_t size, size_t nmemb, void *p_userdata)
{
    size_t realsize = size * nmemb;
    RECV_BUF *p = (RECV_BUF *)p_userdata;
 
    if (p->size + realsize + 1 > p->max_size) {/* hope this rarely happens */ 
        fprintf(stderr, "User buffer is too small, abort...\n");
        abort();
    }

    memcpy(p->buf + p->size, p_recv, realsize); /*copy data from libcurl*/
    p->size += realsize;
    p->buf[p->size] = 0;

    return realsize;
}

/**
 * @brief calculate the actual size of RECV_BUF
 * @param size_t nbytes number of bytes that buf in RECV_BUF struct would hold
 * @return the REDV_BUF member fileds size plus the RECV_BUF buf data size
 */
int sizeof_shm_recv_buf(size_t nbytes)
{
    return (sizeof(RECV_BUF) + sizeof(char) * nbytes);
}

/**
 * @brief initialize the RECV_BUF structure. 
 * @param RECV_BUF *ptr memory allocated by user to hold RECV_BUF struct
 * @param size_t nbytes the RECV_BUF buf data size in bytes
 * NOTE: caller should call sizeof_shm_recv_buf first and then allocate memory.
 *       caller is also responsible for releasing the memory.
 */

int shm_recv_buf_init(RECV_BUF *ptr, size_t nbytes)
{
    if ( ptr == NULL ) {
        return 1;
    }
    
    ptr->buf = (char *)ptr + sizeof(RECV_BUF);
    ptr->size = 0;
    ptr->max_size = nbytes;
    ptr->seq = -1;              /* valid seq should be non-negative */
    
    return 0;
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

/**
 * @brief output data in memory to a file
 * @param path const char *, output file path
 * @param in  void *, input data to be written to the file
 * @param len size_t, length of the input data in bytes
 */

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

void producer(int shmid_stack,  int shmid_sems, int shmid_cnt, int N){

    int count=0;
    ISTACK *stack = shmat(shmid_stack, NULL, 0);
    sem_t *sems = shmat(shmid_sems, NULL, 0);
    int *cnt = shmat(shmid_cnt, NULL, 0);

    RECV_BUF *p_shm_recv_buf = malloc(sizeof(RECV_BUF));
    recv_buf_init(p_shm_recv_buf, MAX_SIZE);
    CURL *curl_handle;
    CURLcode res;
    char url[256];
    
    curl_global_init(CURL_GLOBAL_DEFAULT);

    /* init a curl session */
    curl_handle = curl_easy_init();

    if (curl_handle == NULL) {
        fprintf(stderr, "curl_easy_init: returned NULL\n");
        return;
    }
    
    /* get it! */
    while(count< 50){
        sem_wait(&sems[2]);
        sem_wait(&sems[3]);

        count=*cnt;
        if(count<50)
        {
            *cnt = *cnt + 1 ;
        }
       
        sem_post(&sems[3]);
        if(count<50)
        {
        
            sprintf(url,"http://ece252-%d.uwaterloo.ca:2530/image?img=%d&part=%d",rand()%3+1,N,count);
            
            /* specify URL to get */
            curl_easy_setopt(curl_handle, CURLOPT_URL, url);

            /* register write call back function to process received data */
            curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_cb_curl); 
            /* user defined data structure passed to the call back function */
            curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)p_shm_recv_buf);

            /* register header call back function to process received header data */
            curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_cb_curl); 
            /* user defined data structure passed to the call back function */
            curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, (void *)p_shm_recv_buf);

            /* some servers requires a user-agent field */
            curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
            res = curl_easy_perform(curl_handle);

            if( res != CURLE_OK) {
                fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            }
            
            sem_wait(&sems[0]);
            if(!is_full(stack)){
                push(stack, *p_shm_recv_buf);
            }

            sem_post(&sems[0]);
            sem_post(&sems[1]);
        }
        recv_buf_cleanup(p_shm_recv_buf);
        recv_buf_init(p_shm_recv_buf, MAX_SIZE);
    }
    
    /* cleaning up */
    curl_easy_cleanup(curl_handle);
    curl_global_cleanup();
    recv_buf_cleanup(p_shm_recv_buf);
    free(p_shm_recv_buf);
    return;
}

void consumer(int shmid_stack, int shmid_sems, int shmid_cnt_c, int shmid_def_cnt, int shmid_inflated,int shmid_inf_cnt,int X){

    int count_c=0;
    ISTACK *stack = shmat(shmid_stack, NULL, 0);
    sem_t *sems = shmat(shmid_sems, NULL, 0);
    int *cnt_c = shmat(shmid_cnt_c, NULL, 0);
    U64 *def_cnt = shmat(shmid_def_cnt, NULL, 0);
    U64 *inf_cnt = shmat(shmid_inf_cnt, NULL, 0);
    U8 *inflated = shmat(shmid_inflated, NULL, 0);

    RECV_BUF *p_shm_recv_buf = malloc(sizeof(RECV_BUF));
    recv_buf_init(p_shm_recv_buf, MAX_SIZE);
    
    while(count_c < 50){
           
        sem_wait(&sems[4]);
        if(count_c<50)
        {
            count_c=*cnt_c;
        }
        if(count_c<50)
        {
            *cnt_c = *cnt_c + 1;
        }
        sem_post(&sems[4]);

        if(count_c<50){
            usleep(X);
            sem_wait(&sems[1]);
            sem_wait(&sems[0]);
        
            if(!is_empty(stack)){
                pop(stack, p_shm_recv_buf);
            }

            sem_post(&sems[0]);

            // inflate IDAT data
            unsigned char IDAT_size[4];

            memcpy(&IDAT_size, p_shm_recv_buf->buf + IDAT_position,4);

            U64 IDAT_length = decimal(IDAT_size,4);
            U8 IDAT_data_len[IDAT_length];
            memcpy(&IDAT_data_len, p_shm_recv_buf->buf + data_position,IDAT_length);
            *def_cnt = *def_cnt + IDAT_length;
            
            U64 size = 6 * (400 * 4 + 1);
            U8 dest[size];
            
            mem_inf(dest, &size, IDAT_data_len, IDAT_length);
            
            sem_wait(&sems[4]);
        
            for (int k=0; k < size;k++)
            {
            inflated[size*p_shm_recv_buf->seq+ k] = dest[k];
            }
        
            *inf_cnt=*inf_cnt+size;

            sem_post(&sems[4]);

            sem_post(&sems[2]);
        }

        recv_buf_cleanup(p_shm_recv_buf);
        recv_buf_init(p_shm_recv_buf, MAX_SIZE);
        
    }

    recv_buf_cleanup(p_shm_recv_buf);
    
    // shmdt(stack);
    // shmdt(sems);
    // shmdt(cnt_c);
    // shmdt(def_cnt);
    // shmdt(inflated);
    // shmdt(inf_cnt);
    free(p_shm_recv_buf);

    return;
}

int main( int argc, char** argv ) 
{
    if(argc != 6){
        printf("Need to have six arguments\n");
        return -1;
    }

    int B = atoi(argv[1]);
    int P = atoi(argv[2]);
    int C = atoi(argv[3]);
    int X = atoi(argv[4]);
    int N = atoi(argv[5]);

    //start time
    struct timeval tv;
    double times[2];
    if (gettimeofday(&tv, NULL) != 0) {
        perror("gettimeofday");
        abort();
    }
    times[0] = (tv.tv_sec) + tv.tv_usec/1000000.;

    ISTACK *stack = create_stack(B);

    sem_t* sems;
    int* cnt = malloc(sizeof(int));
    int* cnt_c = malloc(sizeof(int));
    U64* def_cnt = malloc(sizeof(U64));
    U64* inf_cnt = malloc(sizeof(U64));

    U8* inflated;
    int shmid_stack;
    int shmid_sems;
    int shmid_cnt;
    int shmid_cnt_c;
    int shmid_def_cnt;
    int shmid_inflated;
    int shmid_inf_cnt;
    int shm_size = sizeof_shm_stack(B);
    pid_t pid = 0;
    pid_t ppid[P];
    pid_t cpid[C];
   
    inflated = malloc(sizeof(U8) * RAW_SIZE);

    shmid_stack = shmget(IPC_PRIVATE, shm_size, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    shmid_cnt = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    shmid_cnt_c = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    shmid_def_cnt = shmget(IPC_PRIVATE, sizeof(U64), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    shmid_inf_cnt = shmget(IPC_PRIVATE, sizeof(U64), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    shmid_inflated = shmget(IPC_PRIVATE, sizeof(U8) * RAW_SIZE, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    shmid_sems = shmget(IPC_PRIVATE, sizeof(sem_t) * 5, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    
    if ( shmid_stack == -1 || shmid_sems == -1 || shmid_cnt == -1 || 
        shmid_inf_cnt == -1 || shmid_cnt_c == -1 || shmid_def_cnt == -1 || shmid_inflated == -1) {
        perror("shmget");
        abort();
    }

    stack = shmat(shmid_stack, NULL, 0);
    inf_cnt = shmat(shmid_inf_cnt, NULL, 0);
    sems = shmat(shmid_sems, NULL, 0);
    cnt = shmat(shmid_cnt, NULL, 0);
    cnt_c = shmat(shmid_cnt_c, NULL, 0);
    def_cnt = shmat(shmid_def_cnt, NULL, 0);
    inflated = shmat(shmid_inflated, NULL, 0);

    init_shm_stack(stack, B);
    memset(inflated, '\0', sizeof(U8) * RAW_SIZE);

    sem_init(&sems[0], 1, 1);
    sem_init(&sems[1], 1, 0);
    sem_init(&sems[2], 1, B);
    sem_init(&sems[3], 1, 1);
    sem_init(&sems[4], 1, 1);
    *def_cnt = 0;
    *inf_cnt = 0;
    *cnt = 0;
    *cnt_c = 0;

    //producer
    int state_p;
    int state_c;
    
    for(int i = 0; i < C; i++){

            pid = fork();
            
            if(pid == 0){
                consumer(shmid_stack, shmid_sems, shmid_cnt_c, shmid_def_cnt, shmid_inflated, shmid_inf_cnt,X);
                exit(0);
            }
            else if(pid > 0){
                cpid[i] = pid;
            }
            else {
                perror("fork");
                abort();
            }
    }

    if(pid > 0)
    {
        
        for(int i = 0; i < P; i++){

            pid = fork();
            if(pid == 0){
                producer(shmid_stack, shmid_sems, shmid_cnt, N);
                exit(0);
            }
            else if(pid > 0){
                ppid[i] = pid;
            }
            else {
                perror("fork");
                abort();
            }
        }
    }
    if(pid>0)
    {
         for (int i = 0; i < C; i++ ) {

            waitpid(cpid[i], &state_c, 0);
            if (WIFEXITED(state_c)) {
            //    printf("Child cpid[%d]=%d terminated with state: %d.\n", i, cpid[i], state_c);
            }
        }
        for (int i = 0; i < P; i++ ) {

            waitpid(ppid[i], &state_p, 0);
            if (WIFEXITED(state_p)) {
            //    printf("Child ppid[%d]=%d terminated with state: %d.\n", i, ppid[i], state_p);
            }
        }
       
    }

    char first_url[256];
    RECV_BUF recv_buf;
    CURL *curl_handle;
    // CURLcode res;
    sprintf(first_url,"http://ece252-%d.uwaterloo.ca:2530/image?img=%d&part=%d",rand()%3+1,N,0);
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl_handle = curl_easy_init();
    recv_buf_init(&recv_buf, MAX_SIZE);
    /* specify URL to get */
    curl_easy_setopt(curl_handle, CURLOPT_URL, first_url);

    /* register write call back function to process received data */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_cb_curl); 
    /* user defined data structure passed to the call back function */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&recv_buf);

    /* register header call back function to process received header data */
    curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_cb_curl); 
    /* user defined data structure passed to the call back function */
    curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, (void *)&recv_buf);

    /* some servers requires a user-agent field */
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    curl_easy_perform(curl_handle);
  
    
    //    write file
    unsigned char IDAT_data_out[*def_cnt];

    unsigned char IDAT_data[RAW_SIZE];  
    memset(IDAT_data,'\0', sizeof(unsigned char)*(RAW_SIZE));
 
  
    mem_def(IDAT_data_out, def_cnt, inflated, *inf_cnt,Z_BEST_COMPRESSION);
    
    char* output=malloc(sizeof(char)*MAX_SIZE);
    memset(output, '\0',sizeof(char)*MAX_SIZE);
  
    unsigned char png[8]={0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    memcpy(output, &png,8);
  
    /* IHDR */
    
    memcpy(output+IHDR_position, recv_buf.buf+IHDR_position,8);

    U32 IHDR_width=(U32)htonl(400);
    U32 IHDR_height=(U32)htonl(300);

    memcpy(output+IHDR_position+8, &IHDR_width, 4);
    memcpy(output+IHDR_position+12, &IHDR_height, 4);
    
    memcpy(output+IHDR_position+16, recv_buf.buf+IHDR_position+16, 5);
    unsigned char crc1[17];
    memcpy(&crc1,output+IHDR_position+4,17);
    unsigned long crc_IHDR = crc(crc1, 17);
    U32 crc_IHDR_32=(U32)htonl(crc_IHDR);
    memcpy(output+IHDR_position+21,&crc_IHDR_32,4);
  
    /* IDAT */
    U32 def_cnt_32=(U32)htonl(*def_cnt);
    memcpy(output+IDAT_position, &def_cnt_32, 4);
    memcpy(output+IDAT_position+4, recv_buf.buf+IDAT_position+4,4);
    memcpy(output+data_position, &IDAT_data_out, *def_cnt);
    
    unsigned char crc2[*def_cnt+4];
    memcpy(&crc2,output+IDAT_position+4,*def_cnt+4);
    unsigned long crc_IDAT = crc(crc2, *def_cnt+4);
    U32 crc_IDAT_32=(U32)htonl(crc_IDAT);
    memcpy(output+data_position+*def_cnt,&crc_IDAT_32,4);
  
    /* IEND */
    unsigned char input_IDATlength[4];
    memcpy(&input_IDATlength, recv_buf.buf+IDAT_position,4);
    U32 input_IDAT=decimal(input_IDATlength,4);
    memcpy(output+data_position+*def_cnt+4, recv_buf.buf+data_position+input_IDAT+4,8);
    unsigned char crc3[4];
    memcpy(&crc3, output+data_position+*def_cnt+8, 4); 
    unsigned long crc_IEND = crc(crc3, 4);
    U32 crc_IEND_32=(U32)htonl(crc_IEND);
    memcpy(output+data_position+*def_cnt+12, &crc_IEND_32, 4);
    
    char fname[256];
    sprintf(fname, "./all.png");
    write_file(fname, output, *def_cnt+33+12+12);

    if (gettimeofday(&tv, NULL) != 0) {
            perror("gettimeofday");
            abort();
    }
    times[1] = (tv.tv_sec) + tv.tv_usec/1000000.;
    printf("paster2 execution time: %.6lf seconds\n", times[1] - times[0]);
   
    recv_buf_cleanup(&recv_buf);
    shmdt(stack);
    shmdt(inf_cnt);
    shmdt(sems);
    shmdt(cnt);
    shmdt(cnt_c);
    shmdt(def_cnt);
    shmdt(inflated);

    shmctl(shmid_inflated, IPC_RMID, NULL);
    shmctl(shmid_def_cnt, IPC_RMID, NULL);
    shmctl(shmid_cnt_c, IPC_RMID, NULL);
    shmctl(shmid_cnt, IPC_RMID, NULL);
    shmctl(shmid_inf_cnt, IPC_RMID, NULL);
    shmctl(shmid_sems, IPC_RMID, NULL);
    shmctl(shmid_stack, IPC_RMID, NULL);

    // destroy_stack(stack);
    sem_destroy(sems);

    // free(result);
    // free(inflated);
    // free(cnt);
    // free(cnt_c);
    // free(def_cnt);

    return 0;
}
