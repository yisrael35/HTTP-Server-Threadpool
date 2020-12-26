#include "threadpool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
// maximum number of threads allowed in a pool
#define MAXT_IN_POOL 200


/*Yisrael Bar 2/12/20*/

// "dispatch_fn" declares a typed function pointer.  A
// variable of type "dispatch_fn" points to a function
// with the following signature:
// 
//     int dispatch_function(void *arg);
typedef int (*dispatch_fn)(void *);


/**
 * create_threadpool creates a fixed-sized thread
 * pool.  If the function succeeds, it returns a (non-NULL)
 * "threadpool", else it returns NULL.
 * this function should:
 * 1. input sanity check 
 * 2. initialize the threadpool structure
 * 3. initialized mutex and conditional variables
 * 4. create the threads, the thread init function is do_work and its argument is the initialized threadpool. 
 */
threadpool* create_threadpool(int num_threads_in_pool){
    //valdte that 0 < num_threads_in_pool < MAXT_IN_POOL
    if (num_threads_in_pool<=0 || num_threads_in_pool > MAXT_IN_POOL)
    {
        printf("Usage: threadpool <pool-size> <max-number-of-jobs>\n");
        return NULL;
    }
    threadpool *t1 = (threadpool*)malloc(sizeof(threadpool));
    if (t1 == NULL)
    {
        printf("ERROR with threadpool malloc in create_threadpool\n");
       return NULL; 
    }
    //init all t1 values
    t1->num_threads = num_threads_in_pool;
    t1->qsize = 0;
    t1->qhead = NULL;
    t1->qtail = NULL;
    t1->shutdown = 0;
    t1->dont_accept = 0;
// init all lock's
    if (pthread_mutex_init(&t1->qlock, NULL) != 0) {                                    
        perror("pthread_mutex_init() error");                                        
        return NULL;                                                                
    }
    if (pthread_cond_init(&t1->q_not_empty, NULL) != 0) {                                    
        perror("pthread_cond_init() error");                                        
        return NULL;                                                              
    }
    if (pthread_cond_init(&t1->q_empty, NULL) != 0) {                                    
        perror("pthread_cond_init() error");                                        
        return NULL;                                                                 
    }  

    //allocate the threads
    t1->threads = (pthread_t *)malloc(num_threads_in_pool* sizeof(pthread_t));
    if (t1->threads ==  NULL)
    {
        printf("ERROR with threads malloc in create_threadpool\n");
        free(t1);
        return NULL; 
    }
    //create the threades 
    int status = 0;
    for (int i = 0; i < num_threads_in_pool; i++)
    {
        status = pthread_create(&t1->threads[i],NULL , do_work, t1 );
        if (status!=0)
        {
            fputs("pthread create failed ", stderr);
            return NULL;
        }  
    }
    
    return t1;
}


/**
 * dispatch enter a "job" of type work_t into the queue.
 * when an available thread takes a job from the queue, it will
 * call the function "dispatch_to_here" with argument "arg".
 * this function should:
 * 1. create and init work_t element
 * 2. lock the mutex
 * 3. add the work_t element to the queue
 * 4. unlock mutex
 *
 */
typedef struct work_st work_t;
void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg){
    if (from_me->dont_accept == 1)
    {
        printf("destroy function has begun - can't accept new job\n");
        return;
    }
    
//  init work_t element
    work_t * w1 = (work_t*)malloc(sizeof(work_t));
    w1->routine = dispatch_to_here;
    w1->arg = arg;
    w1->next = NULL;
    //lock befor update from_me- threadpool
    pthread_mutex_lock(&from_me->qlock); 
    //update the queue
    if(from_me->qsize != 0){
        from_me->qtail->next = w1;
        from_me->qtail = w1;
	}
	else{
		from_me->qhead = w1;
		from_me->qtail = w1;
	}
    from_me->qsize++;
    pthread_mutex_unlock(&from_me->qlock); 
    //raise a signal to say there is a new job
    if (pthread_cond_signal(&from_me->q_not_empty) != 0) {                                       
        perror("pthread_cond_signal() error");                                     
        return;                                                                   
    }
}




/**
 * The work function of the thread
 * this function should:
 * 1. lock mutex
 * 2. if the queue is empty, wait
 * 3. take the first element from the queue (work_t)
 * 4. unlock mutex
 * 5. call the thread routine
 *
 */
void* do_work(void* p){
    if(p == NULL)
		return NULL;
    threadpool *t1 = p;
        //aquire the lock

    while (1)
    {
        // printf("queue size: %d \n", t1->qsize);
        pthread_mutex_lock(&t1->qlock);
        //-----in case distroction had began
        if (t1->shutdown == 1)
        {   
        	pthread_mutex_unlock(&(t1->qlock));
            return NULL;
        }
        //check if the queue is empty - go to sleep
        if (t1->qsize == 0)
        {
            if (pthread_cond_wait(&t1->q_not_empty,&t1->qlock) != 0) {                                       
                perror("pthread_cond_wait() error");                                     
                return NULL;                                                                  
            }     
        }
        if (t1->qsize == 0 && t1->dont_accept == 1)
            {
               if (pthread_cond_signal(&t1->q_empty) != 0) {                                       
                    perror("pthread_cond_signal() error");                                     
                    return NULL;                                                                 
                } 
            }
        if (t1->shutdown == 1)
        {    
            //in case distroction had began
        	pthread_mutex_unlock(&(t1->qlock));
            // pthread_exit(NULL);
            return NULL;
        
            
        }else
        {    
            // if queue is not empty=>take assignment
            work_t * w1 = t1->qhead;
            if(t1->qsize == 1){
                t1->qhead = NULL;
                t1->qtail = NULL;
            }
            else{
                if(w1 != NULL){
                    t1->qhead = t1->qhead->next;
                }
                else{
                    // printf("there is a problame\n");
                } 
            }
            t1->qsize--;
            
            
            
            //realse th lock
            pthread_mutex_unlock(&t1->qlock);
            if(w1 != NULL){
                w1->routine(w1->arg);
                free(w1);
            }


        }
        if (t1->qsize == 0 && t1->dont_accept == 1)
        {
            if (pthread_cond_signal(&t1->q_empty) != 0) {                                       
                perror("pthread_cond_signal() error");                                     
                return NULL;                                                                
            } 
        }
    }
    

  return NULL;

}


/**
 * destroy_threadpool kills the threadpool, causing
 * all threads in it to commit suicide, and then
 * frees all the memory associated with the threadpool.
 */
void destroy_threadpool(threadpool* destroyme){
    //LOCK
	if(pthread_mutex_lock(&destroyme->qlock)!=0){
        perror("pthread_mutex_lock() error");                                     
        return;
	}
    destroyme->dont_accept = 1;
    //wait for signal that queue is empty and can't get more job
    if (pthread_cond_wait(&destroyme->q_empty,&destroyme->qlock) != 0) {                                       
        perror("pthread_cond_wait() error");                                     
        return;                                                                    
    }
    //chenge shutdown flag to 1 - when thread see it he know to exit..
    destroyme->shutdown = 1;
    // printf("destroction has begun ...\n");

	//unlock
	if(pthread_mutex_unlock(&destroyme->qlock)!=0){
        perror("pthread_mutex_unlock() error");                                     
        return;
	}
    //wake all the threads so they find out that -->
    // --> shoudown is 0 and they will kill them self's
    if (pthread_cond_broadcast(&destroyme->q_not_empty) != 0) {                                     
        perror("pthread_cond_broadcast() error");                                   
        return;                                                                 
    }     
    int rc = 0;
    //collect all the zombie's
    for (int i = 0; i < destroyme->num_threads; i++)
    {   
        rc = pthread_join(destroyme->threads[i],NULL);
        if(rc){
            printf("error in pthread join\n");
            return;
        }
    }
    //delete the lock
    if (pthread_mutex_destroy(&destroyme->qlock) != 0) {                                       
        perror("pthread_cond_destroy() error");                                     
        return;                                                                   
    }
    if (pthread_cond_destroy(&destroyme->q_not_empty) != 0) {                                       
        perror("pthread_cond_destroy() error");                                     
        return;                                                                    
    }
    if (pthread_cond_destroy(&destroyme->q_empty) != 0) {                                       
        perror("pthread_cond_destroy() error");                                     
        return;                                                                    
    }
    //clean all the dynamic memory - malloc
    free(destroyme->threads);
    free(destroyme);
    
}

