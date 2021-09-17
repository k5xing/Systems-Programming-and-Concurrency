#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <search.h>
#include <unistd.h>
#include <curl/multi.h>
#include <libxml/HTMLparser.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/uri.h>
#include "function.c"


#define SEED_URL "http://ece252-1.uwaterloo.ca/lab4/"
#define ECE252_HEADER "X-Ece252-Fragment: "
#define BUF_SIZE 1048576  /* 1024*1024 = 1M */
#define BUF_INC  524288   /* 1024*512  = 0.5M */
#define MAX_WAIT_MSECS 30*1000
#define CT_PNG  "image/png"
#define CT_HTML "text/html"
#define CT_PNG_LEN  9
#define CT_HTML_LEN 9


#define max(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

int main( int argc, char** argv ) 
{
    int *png_cnt = malloc(sizeof(int));
    memset(png_cnt, 0, sizeof(int));
    int num_conn = 1;
    int num_picture = 50;
    int check_logfile = 0;
    char logfile[256];
    node_t *tosee_list;
    struct hsearch_data *visited_list;
    node_t *png_list;
    node_t *curr;
    node_t *log_list;

    CURLM *cm=NULL;
    CURL *eh=NULL;
    CURLMsg *msg=NULL;
    CURLcode return_code=0;
    int still_running=0, i=0, msgs_left=0;
    int c;
    int arg_num = 0;
    char url[256];

    RECV_BUF *recv_bufs = malloc(sizeof(RECV_BUF) * 500);
    char *str = "option requires an argument";

    while ((c = getopt (argc, argv, "t:m:v:")) != -1) {
        switch (c) {
            case 't':
            num_conn = strtoul(optarg, NULL, 10);
            arg_num++;
            
            if (num_conn <= 0) {
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

    tosee_list = malloc(sizeof(node_t));
    tosee_list->png_url=url;
    tosee_list->next=NULL;
    visited_list = calloc(1,sizeof(struct hsearch_data));
    png_list = malloc(sizeof(node_t));
    png_list=NULL;
    log_list = malloc(sizeof(node_t));
    log_list=NULL;
    hcreate_r(500, visited_list);

    //start time
    struct timeval tv;
    double times[2];
    if (gettimeofday(&tv, NULL) != 0) {
        perror("gettimeofday");
        abort();
    }
    times[0] = (tv.tv_sec) + tv.tv_usec/1000000.;
    
    curl_global_init(CURL_GLOBAL_DEFAULT);
    cm = curl_multi_init();
    
    /* process the download data */
    curr = tosee_list;

    curl_multi_setopt(cm, CURLMOPT_MAX_TOTAL_CONNECTIONS, (long)num_conn);

    recv_buf_init(&recv_bufs[i],BUF_SIZE);
    init(cm, &recv_bufs[i], url);
    hadd(visited_list, url);
    i++;
    curl_multi_perform(cm,&still_running);
    RECV_BUF* recv_buf=malloc(sizeof(RECV_BUF));

    while((*png_cnt) < num_picture && still_running > 0){
        int numfds=0;
        int res = curl_multi_wait(cm, NULL, 0, MAX_WAIT_MSECS, &numfds);
        if(res != CURLM_OK) {
            //fprintf(stderr, "error: curl_multi_wait() returned %d\n", res);
            return EXIT_FAILURE;
        }
        curl_multi_perform(cm, &still_running);

        while ((msg = curl_multi_info_read(cm, &msgs_left)) && (*png_cnt) < num_picture) {
            
            if (msg->msg == CURLMSG_DONE) {
               
                eh = msg->easy_handle;
                return_code = msg->data.result;
                recv_buf_init(recv_buf,BUF_SIZE);
                curl_easy_getinfo(eh, CURLINFO_PRIVATE, &(recv_buf));
                if(check_logfile){
                    push(&log_list, recv_buf->url);
                }
                if(return_code!=CURLE_OK) {
                    // fprintf(stderr, "CURL error code: %d\n", msg->data.result);
                    curl_multi_remove_handle(cm, eh);
                    curl_easy_cleanup(eh);
                    recv_buf_cleanup(recv_buf);
                    continue;
                }

                //call process data with recv_buf.buf
                process_data(eh, recv_buf, &png_list, png_cnt, num_picture, visited_list, &curr);

                //add more curl_handle
                while(curr!=NULL)
                {
                    char* element = pop(&curr);

                    int found = hfind(visited_list,element);
                    if(found == -1) 
                    {
                        recv_buf_init(&recv_bufs[i],BUF_SIZE);
                        hadd(visited_list, element);
                        init(cm, &recv_bufs[i], element);
                        i++;
                        curl_multi_perform(cm, &still_running);
                    }
                }
                curl_multi_remove_handle(cm, eh);
                curl_easy_cleanup(eh);
                recv_buf_cleanup(recv_buf);
                
            }
            
        } 
    }

    //print logfile
    if(check_logfile) print_node(log_list, logfile);

    //print png file
    print_node(png_list, "png_urls.txt");
    //end time
    if (gettimeofday(&tv, NULL) != 0) {
        perror("gettimeofday");
        abort();
    }
    times[1] = (tv.tv_sec) + tv.tv_usec/1000000.;
    printf("findpng3 execution time: %.6lf seconds\n", times[1] - times[0]);
    /* cleaning up */
    free(png_cnt);
    free(recv_bufs);
    free_list(png_list);
    free_list(log_list);
    // free(recv_buf);
    hdestroy_r(visited_list);
    free(visited_list);
    curl_multi_cleanup(cm);
    curl_global_cleanup();
    xmlCleanupParser();
    
    return 0;
}