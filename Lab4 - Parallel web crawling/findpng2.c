#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <search.h>
#include <unistd.h>
#include <curl/curl.h>
#include <libxml/HTMLparser.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/uri.h>
#include <semaphore.h>
#include <pthread.h>
#include "linked_list.c"


#define SEED_URL "http://ece252-1.uwaterloo.ca/lab4/"
#define ECE252_HEADER "X-Ece252-Fragment: "
#define BUF_SIZE 1048576  /* 1024*1024 = 1M */
#define BUF_INC  524288   /* 1024*512  = 0.5M */

#define CT_PNG  "image/png"
#define CT_HTML "text/html"
#define CT_PNG_LEN  9
#define CT_HTML_LEN 9


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

int png_cnt=0;
int url_cnt=0;
int tosee_cnt=1;
int num_threads = 1;
int num_picture = 50;
int check_logfile = 0;
char logfile[256];
int wait_cnt = 0;
node_t *tosee_list;
struct hsearch_data *visited_list;
node_t *png_list;
node_t *curr;
node_t *log_list;
pthread_mutex_t tosee_lock;
pthread_mutex_t png_lock;
pthread_mutex_t log_lock;
pthread_rwlock_t visited_lock;
pthread_cond_t tosee_cond;

htmlDocPtr mem_getdoc(char *buf, int size, const char *url);
xmlXPathObjectPtr getnodeset (xmlDocPtr doc, xmlChar *xpath);
int find_http(char *fname, int size, int follow_relative_links, const char *base_url);
size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata);
size_t write_cb_curl3(char *p_recv, size_t size, size_t nmemb, void *p_userdata);
int recv_buf_init(RECV_BUF *ptr, size_t max_size);
int recv_buf_cleanup(RECV_BUF *ptr);
void cleanup(CURL *curl, RECV_BUF *ptr);
int write_file(const char *path, const void *in, size_t len);
CURL *easy_handle_init(RECV_BUF *ptr, const char *url);
int process_data(CURL *curl_handle, RECV_BUF *p_recv_buf);


htmlDocPtr mem_getdoc(char *buf, int size, const char *url)
{
    int opts = HTML_PARSE_NOBLANKS | HTML_PARSE_NOERROR | \
               HTML_PARSE_NOWARNING | HTML_PARSE_NONET;
    htmlDocPtr doc = htmlReadMemory(buf, size, url, NULL, opts);
    
    if ( doc == NULL ) {
       // fprintf(stderr, "Document not parsed successfully.\n");
        return NULL;
    }
    return doc;
}

xmlXPathObjectPtr getnodeset (xmlDocPtr doc, xmlChar *xpath)
{
	
    xmlXPathContextPtr context;
    xmlXPathObjectPtr result;

    context = xmlXPathNewContext(doc);
    if (context == NULL) {
        //printf("Error in xmlXPathNewContext\n");
        return NULL;
    }
    result = xmlXPathEvalExpression(xpath, context);
    xmlXPathFreeContext(context);
    if (result == NULL) {
       //printf("Error in xmlXPathEvalExpression\n");
        return NULL;
    }
    if(xmlXPathNodeSetIsEmpty(result->nodesetval)){
        xmlXPathFreeObject(result);
        //printf("No result\n");
        return NULL;
    }
    return result;
}
void hadd(struct hsearch_data *tab, char *key)
{
    ENTRY item = {key};
    ENTRY *pitem = &item;

    if (hsearch_r(item, ENTER, &pitem, tab)) {
        
    }
}

int hfind(struct hsearch_data *tab, char *key)
{
    ENTRY item = {key};
    ENTRY *pitem = &item;
    if (hsearch_r(item, FIND, &pitem, tab)) {
        return 0;
    }

    return -1;
}
int find_http(char *buf, int size, int follow_relative_links, const char *base_url)
{

    int i;
    htmlDocPtr doc;
    xmlChar *xpath = (xmlChar*) "//a/@href";
    xmlNodeSetPtr nodeset;
    xmlXPathObjectPtr result;
    xmlChar *href;
		
    if (buf == NULL) {
        return 1;
    }
    doc = mem_getdoc(buf, size, base_url);
    result = getnodeset (doc, xpath);
    if (result) {
        nodeset = result->nodesetval;
        for (i=0; i < nodeset->nodeNr; i++) {
           
            href = xmlNodeListGetString(doc, nodeset->nodeTab[i]->xmlChildrenNode, 1);
            if ( follow_relative_links ) {

                xmlChar *old = href;
                href = xmlBuildURI(href, (xmlChar *) base_url);
                xmlFree(old);
            }
            if ( href != NULL && !strncmp((const char *)href, "http", 4) ) {
                pthread_rwlock_rdlock(&visited_lock);
                int find = hfind(visited_list, (char*)href);
                pthread_rwlock_unlock(&visited_lock);
               
                if(find == -1){
                    
                    pthread_mutex_lock(&tosee_lock);
                    push(&curr, (char*)href);
                    tosee_cnt++;
                    pthread_cond_signal(&tosee_cond);
                    pthread_mutex_unlock(&tosee_lock);
                }
               
            }
              
            xmlFree(href);
        }
        xmlXPathFreeObject (result);
    }
    xmlFreeDoc(doc);
    
    return 0;
}
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

#ifdef DEBUG1_
    printf("%s", p_recv);
#endif /* DEBUG1_ */
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
    ptr->seq = -1;              /* valid seq should be positive */
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

void cleanup(CURL *curl, RECV_BUF *ptr)
{
    curl_easy_cleanup(curl);
    recv_buf_cleanup(ptr);
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

/**
 * @brief create a curl easy handle and set the options.
 * @param RECV_BUF *ptr points to user data needed by the curl write call back function
 * @param const char *url is the target url to fetch resoruce
 * @return a valid CURL * handle upon sucess; NULL otherwise
 * Note: the caller is responsbile for cleaning the returned curl handle
 */

CURL *easy_handle_init(RECV_BUF *ptr, const char *url)
{
    CURL *curl_handle = NULL;

    if ( ptr == NULL || url == NULL) {
        return NULL;
    }

    /* init user defined call back function buffer */
    if ( recv_buf_init(ptr, BUF_SIZE) != 0 ) {
        return NULL;
    }
    /* init a curl session */
    curl_handle = curl_easy_init();

    if (curl_handle == NULL) {
        fprintf(stderr, "curl_easy_init: returned NULL\n");
        return NULL;
    }

    /* specify URL to get */
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);

    /* register write call back function to process received data */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_cb_curl3); 
    /* user defined data structure passed to the call back function */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)ptr);

    /* register header call back function to process received header data */
    curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_cb_curl); 
    /* user defined data structure passed to the call back function */
    curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, (void *)ptr);

    /* some servers requires a user-agent field */
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "ece252 lab4 crawler");

    /* follow HTTP 3XX redirects */
    curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
    /* continue to send authentication credentials when following locations */
    curl_easy_setopt(curl_handle, CURLOPT_UNRESTRICTED_AUTH, 1L);
    /* max numbre of redirects to follow sets to 5 */
    curl_easy_setopt(curl_handle, CURLOPT_MAXREDIRS, 5L);
    /* supports all built-in encodings */ 
    curl_easy_setopt(curl_handle, CURLOPT_ACCEPT_ENCODING, "");

    /* Max time in seconds that the connection phase to the server to take */
    //curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, 5L);
    /* Max time in seconds that libcurl transfer operation is allowed to take */
    //curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 10L);
    /* Time out for Expect: 100-continue response in milliseconds */
    //curl_easy_setopt(curl_handle, CURLOPT_EXPECT_100_TIMEOUT_MS, 0L);

    /* Enable the cookie engine without reading any initial cookies */
    curl_easy_setopt(curl_handle, CURLOPT_COOKIEFILE, "");
    /* allow whatever auth the proxy speaks */
    curl_easy_setopt(curl_handle, CURLOPT_PROXYAUTH, CURLAUTH_ANY);
    /* allow whatever auth the server speaks */
    curl_easy_setopt(curl_handle, CURLOPT_HTTPAUTH, CURLAUTH_ANY);

    return curl_handle;
}

int check_png(RECV_BUF *p_recv_buf){
  
  unsigned char png_format[8]={0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
  if(memcmp(png_format,p_recv_buf->buf,8))
  {
    return 0;
  }
  else{
    //if correct
    return 1;
  }
}

int process_html(CURL *curl_handle, RECV_BUF *p_recv_buf)
{
    int follow_relative_link = 1;
    char *url = NULL; 
   
    curl_easy_getinfo(curl_handle, CURLINFO_EFFECTIVE_URL, &url);
    find_http(p_recv_buf->buf, p_recv_buf->size, follow_relative_link, url); 

    return 0;
}

int process_png(CURL *curl_handle, RECV_BUF *p_recv_buf)
{
   
    char *eurl = NULL;          /* effective URL */
    curl_easy_getinfo(curl_handle, CURLINFO_EFFECTIVE_URL, &eurl);
    if ( eurl != NULL && check_png (p_recv_buf)) {

        pthread_mutex_lock(&png_lock);
        if(png_cnt<num_picture){
            push(&png_list, eurl);
            png_cnt++;
        }
        pthread_mutex_unlock(&png_lock);
       
        return 1;
    }
   
    return 0;
}
/**
 * @brief process teh download data by curl
 * @param CURL *curl_handle is the curl handler
 * @param RECV_BUF p_recv_buf contains the received data. 
 * @return 0 on success; non-zero otherwise
 */

int process_data(CURL *curl_handle, RECV_BUF *p_recv_buf)
{
    CURLcode res;
    long response_code;
    res = curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &response_code);
    if ( res == CURLE_OK ) {
	//    printf("Response code: %ld\n", response_code);
    }

    if ( response_code >= 400 ) { 
    //	fprintf(stderr, "Error.\n");
        return 1;
    }

    char *ct = NULL;
    res = curl_easy_getinfo(curl_handle, CURLINFO_CONTENT_TYPE, &ct);
    if ( res == CURLE_OK && ct != NULL ) {
    	//printf("Content-Type: %s, len=%ld\n", ct, strlen(ct));
    } else {
       // fprintf(stderr, "Failed obtain Content-Type\n");
        return 2;
    }

    if ( strstr(ct, CT_HTML) ) {
        return process_html(curl_handle, p_recv_buf);
    } else if ( strstr(ct, CT_PNG) ) {
        return process_png(curl_handle, p_recv_buf);
    } else {}

    return 0;
}

void *crawler(void *ignore){
    CURL *curl_handle;
    CURLcode res;
    RECV_BUF recv_buf;
    int count=0;
    while(1){
        
        pthread_mutex_lock(&png_lock);
        count=png_cnt;
        pthread_mutex_unlock(&png_lock);

        if(count >= num_picture){
            pthread_mutex_lock(&tosee_lock);
            wait_cnt++;
            if(wait_cnt == num_threads){
                pthread_cond_broadcast(&tosee_cond);
            }
            pthread_mutex_unlock(&tosee_lock);
            break;
        }
        if(count<num_picture)
        {
            pthread_mutex_lock(&tosee_lock);
            if(is_empty(curr)||wait_cnt!=0){
                wait_cnt++;
                if(wait_cnt < num_threads){
                    pthread_cond_wait(&tosee_cond, &tosee_lock);
                }
                pthread_mutex_lock(&png_lock);
                count=png_cnt;
                pthread_mutex_unlock(&png_lock);
                if((wait_cnt == num_threads && is_empty(curr))||(wait_cnt == num_threads && count>=num_picture)){
                    pthread_cond_broadcast(&tosee_cond);
                    pthread_mutex_unlock(&tosee_lock);
                    break;
                }
                wait_cnt--;
            }
            // if(wait_cnt==num_threads){
            //     pthread_cond_broadcast(&tosee_cond);
            // }
            char* element = pop(&curr);
            tosee_cnt--;
            pthread_mutex_unlock(&tosee_lock);
            if(element==NULL) 
            {
                continue;
            }
            // pthread_rwlock_rdlock(&visited_lock);
            // int find = hfind(visited_list, element);
            // pthread_rwlock_unlock(&visited_lock);

            //if(find == -1){
                pthread_rwlock_wrlock(&visited_lock);
                if(hfind(visited_list,element)!=-1) 
                {   
                    pthread_rwlock_unlock(&visited_lock);
                    continue;
                }
                hadd(visited_list, element);
                url_cnt++;
                pthread_rwlock_unlock(&visited_lock);
                pthread_mutex_lock(&log_lock);
                if(check_logfile){
                    push(&log_list,element);
                }
                pthread_mutex_unlock(&log_lock);
                curl_handle = easy_handle_init(&recv_buf, element);
                if ( curl_handle == NULL ) {
                    // fprintf(stderr, "Curl initialization failed. Exiting...\n");
                    curl_global_cleanup();
                    abort();
                }
                /* get it! */
                res = curl_easy_perform(curl_handle);

                if( res != CURLE_OK) {
                   // fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
                } else {
               // printf("%lu bytes received in memory %p, seq=%d.\n", recv_buf.size, recv_buf.buf, recv_buf.seq);
                    process_data(curl_handle, &recv_buf);
                }
                cleanup(curl_handle, &recv_buf);
           // }
            
          
        }
    }
    return NULL;
}

int main( int argc, char** argv ) 
{
    
    int c;
    int arg_num = 0;
    CURL *curl_handle;
    CURLcode res;
    char url[256];
    RECV_BUF recv_buf;
    char *str = "option requires an argument";

    while ((c = getopt (argc, argv, "t:m:v:")) != -1) {
        switch (c) {
            case 't':
            num_threads = strtoul(optarg, NULL, 10);
            arg_num++;
            
            if (num_threads <= 0) {
                fprintf(stderr, "%s: %s > 0 -- 't'\n", argv[0], str);
                return -1;
            }
            break;
            case 'm':
            num_picture = strtoul(optarg, NULL, 10);
            arg_num++;
            if (num_picture <= 0) {
                fprintf(stderr, "%s: %s > 0 -- 'm'\n", argv[0], str);
                return -1;
            }
            break;
            case 'v':
            strcpy(logfile, optarg);
            check_logfile = 1;
            arg_num++;
            if(strstr(logfile, ".txt") == NULL){
                fprintf(stderr, "%s: %s should be a txt file -- 'v'\n", argv[0], str);
            }
            break;
            default:
            return -1;
        }
    }
    
    if(argc%2 == 0){
        strcpy(url, argv[arg_num*2+1]);
       
    } else{
        strcpy(url, SEED_URL); 
    }

    pthread_t web_crawler[num_threads];
    tosee_list = malloc(sizeof(node_t));
    tosee_list->png_url=url;
    tosee_list->next=NULL;
    visited_list = calloc(1,sizeof(struct hsearch_data));
    png_list = malloc(sizeof(node_t));
    png_list=NULL;
    log_list = malloc(sizeof(node_t));
    log_list=NULL;
    hcreate_r(500, visited_list);
    pthread_mutex_init(&tosee_lock, NULL);
    pthread_mutex_init(&png_lock, NULL);
    pthread_mutex_init(&log_lock, NULL);
    pthread_rwlock_init(&visited_lock, NULL);
    pthread_cond_init(&tosee_cond, NULL);

    //start time
    struct timeval tv;
    double times[2];
    if (gettimeofday(&tv, NULL) != 0) {
        perror("gettimeofday");
        abort();
    }
    times[0] = (tv.tv_sec) + tv.tv_usec/1000000.;
    
    curl_global_init(CURL_GLOBAL_DEFAULT);
    /* process the download data */
    curr = tosee_list;
    if(num_threads == 1){
        while(png_cnt<num_picture && !is_empty(curr)){
            curl_handle = easy_handle_init(&recv_buf, curr->png_url);
            char* element = pop(&curr);
            int find = hfind(visited_list, element);
        
            if(find == -1){
                hadd(visited_list, element);
                url_cnt++;
                if(check_logfile){
                    push(&log_list,element);
                }
                if ( curl_handle == NULL ) {
                //  fprintf(stderr, "Curl initialization failed. Exiting...\n");
                    curl_global_cleanup();
                    abort();
                }
                /* get it! */
                res = curl_easy_perform(curl_handle);

                if( res != CURLE_OK) {
                //   fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));

                } else {
                // printf("%lu bytes received in memory %p, seq=%d.\n", recv_buf.size, recv_buf.buf, recv_buf.seq);
                        process_data(curl_handle, &recv_buf);
                }
            }
            
            cleanup(curl_handle, &recv_buf);
        }
    }
    else{
        for(int i = 0; i < num_threads; i++){
            pthread_create(&web_crawler[i], NULL, crawler, NULL);
        }
        for(int j = 0; j < num_threads; j++){
            pthread_join(web_crawler[j], NULL);
        }
    }


    //print logfile
    if(check_logfile) print_node(log_list, logfile);
    //print png file
    print_node(png_list, "png_urls.txt");
    
    // printf("png_cnt: %d\n", png_cnt);
    // printf("url_cnt: %d\n", url_cnt);
    
    //end time
    if (gettimeofday(&tv, NULL) != 0) {
        perror("gettimeofday");
        abort();
    }
    times[1] = (tv.tv_sec) + tv.tv_usec/1000000.;
    printf("findpng2 execution time: %.6lf seconds\n", times[1] - times[0]);
    /* cleaning up */

    free_list(png_list);
    free_list(log_list);

    hdestroy_r(visited_list);
    pthread_mutex_destroy(&tosee_lock);
    pthread_mutex_destroy(&png_lock);
    pthread_mutex_destroy(&log_lock);
    pthread_rwlock_destroy(&visited_lock);
    pthread_cond_destroy(&tosee_cond);
    curl_global_cleanup();
    xmlCleanupParser();
    
    return 0;
}