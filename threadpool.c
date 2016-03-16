//Lior Sapir I.D. 304916562
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "threadpool.h"

/**
 * create_threadpool creates a fixed-sized thread
 * pool.  If the function succeeds, it returns a (non-NULL)
 * "threadpool", else it returns NULL.
 * this function should:
 * 1. input sanity check - done
 * 2. initialize the threadpool structure - done
 * 3. initialized mutex and conditional variables - done
 * 4. create the threads, the thread init function is do_work and its argument is
 			the initialized threadpool. -done
 */

threadpool* create_threadpool(int num_threads_in_pool)
{
	if(num_threads_in_pool <= 0 || num_threads_in_pool > MAXT_IN_POOL)
	{
		perror("\nthreads number no legal\n");
		return NULL;
	}
	threadpool *myPool = (threadpool*)calloc(1,sizeof(threadpool));
	if(myPool == NULL)
	{
		perror("\ncalloc failure\n");
		return NULL;
	}

	myPool->num_threads = num_threads_in_pool;
	myPool->qsize = 0;

	myPool->threads = (pthread_t*)calloc(num_threads_in_pool, sizeof(pthread_t));
	if(myPool->threads == NULL)
	{
		perror("\ncalloc failure\n");
		return NULL;
	}
	myPool->qhead = NULL;
	myPool->qtail = NULL;

	//////////---------initial mutex----------///////////
	if(pthread_mutex_init(&(myPool->qlock),NULL) != 0)
	{
		perror("\nmutex failure\n");
		return NULL;
	}
	////--------initial condition variables------////////

	if(pthread_cond_init(&(myPool->q_empty),NULL) != 0)
	{
		perror("\nmutex failure\n");
		return NULL;
	}
	if(pthread_cond_init(&(myPool->q_not_empty),NULL) != 0)
	{
		perror("\nmutex failure\n");
		return NULL;
	}

	myPool->dont_accept = 0;
	myPool->shutdown = 0;

	int i;
	for ( i = 0; i < num_threads_in_pool; i++)
	{
		int suc = pthread_create(&(myPool->threads[i]), NULL, do_work, myPool);
		if(suc != 0)
		{
				perror("\nthread failure\n");
				return NULL;
		}
	}
	return (threadpool*)myPool;
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
*/
void dispatch(threadpool* from_me, dispatch_fn dispatch_to_here, void *arg)
{
	if(from_me == NULL)
	{
			perror("\narguments failure: from_me\n");
			return;
	}
	if(dispatch_to_here == NULL)
	{
			perror("\narguments failure: dispatch_to_here\n");
			return ;
	}
	if(arg == NULL)
	{
			perror("\narguments failure: arg\n");
			return ;
	}
	//////////////////////////////////////////////////////////////////////////////
	//-----If destroy function has begun, don't accept new item to the queue--////
	//////////////////////////////////////////////////////////////////////////////

	if(from_me->dont_accept)
	{
			return;
	}
	//////////////////////////////////////////////////////////////////////////////
	////---Create work_t structure and init it with the routine and argument--////
	//////////////////////////////////////////////////////////////////////////////


	work_t *work = (work_t*)calloc(1,sizeof(work_t));
	if(work  == NULL)
	{
			perror("\ncalloc failure\n");
			return;
	}
	work->arg = arg;
	work->routine  = dispatch_to_here;
	work->next = NULL;

	//////////////////////////////////////////////////////////////////////////////
	////--------------------Add item to the queue-----------------------------////
	//////////////////////////////////////////////////////////////////////////////
	if(pthread_mutex_lock(&(from_me->qlock)) != 0)
	{
			perror("\nmutex failure\n");
			return;
	}
	if(from_me->qhead == NULL)
	{
			from_me->qhead = work;
			from_me->qtail = work;
	}
	else
	{
			from_me->qtail->next = work;
			from_me->qtail = from_me->qtail->next;
	}
	from_me->qsize++;
	//////////////////////////////////////////////////////////////////////////////
	////------------------unlock mutex and signal the empty-------------------////
	//////////////////////////////////////////////////////////////////////////////
	if(pthread_cond_signal(&(from_me->q_empty)) != 0)
	{
			perror("\ncalloc failure\n");
			return;
	}
	if(pthread_mutex_unlock(&(from_me->qlock)) != 0)
	{
			perror("\ncalloc failure\n");
			return;
	}
}


/*
do_work should run in an endless loop and:
1. If destruction process has begun, exit thread
2. If the queue is empty, wait (no job to make)
3. Check again destruction flag.
4. Take the first element from the queue (*work_t)
5. If the queue becomes empty and destruction process wait to begin, signal
destruction process.
6. Call the thread routine.
*/
void* do_work(void* p)
{
		if(p == NULL)
		{
				perror("\narguments failure\n");
				return NULL;
		}
		threadpool  *tempPoll = (threadpool*)p;
		while(1)
		{
				if(pthread_mutex_lock(&(tempPoll->qlock)) != 0)
				{
						perror("\nmutex failure\n");
						return NULL;
				}

				if(tempPoll->shutdown)
				{
					if(pthread_mutex_unlock(&(tempPoll->qlock)) != 0)
					{
							perror("\nmutex failure\n");
							return NULL;
					}
					return NULL;
				}
				// wait for job or function
				// if the queue is empty, wait
				while(tempPoll->qsize == 0)
				{
						if(pthread_cond_wait(&(tempPoll->q_empty), &(tempPoll->qlock)) != 0)
						{
								perror("\ncondition failure\n");
								return NULL;
						}
						//Check again destruction flag.
						if(tempPoll->shutdown)
						{
								if(pthread_mutex_unlock(&(tempPoll->qlock)) != 0)
								{
										perror("\nmutex failure\n");
										return NULL;
								}
								return NULL;
						}
				}

				//take the first element from the queue (work_t)
				work_t *toTake = tempPoll->qhead;
				void *in = (void*)(toTake->arg);
				//in case of one work
				if((tempPoll->qhead)->next == NULL)
				{
						tempPoll->qhead = NULL;
						tempPoll->qtail = NULL;
				}
				else //in case of more
						tempPoll->qhead = (tempPoll->qhead)->next;
				tempPoll->qsize--;
				// If the queue becomes empty and destruction process wait to begin, signal
				// destruction process.

				if(tempPoll->qsize == 0)
				{
						if(pthread_cond_signal(&(tempPoll->q_not_empty)) != 0)
						{
								perror("\ncondition failure\n");
								return NULL;
						}
				}
				//Call the thread routine.
				if(pthread_mutex_unlock(&(tempPoll->qlock)) != 0)
				{
						perror("\nmutex failure\n");
						return NULL;
				}
				(toTake->routine)(in);
				free(toTake);
		}
}
/* from pdf
1. Set don’t_accept flag to 1
2. Wait for queue to become empty
3. Set shutdown flag to 1
4. Signal threads that wait on empty queue, so they can wake up, see shutdown flag
and exit.
5. Join all threads
6. Free whatever you have to free.
*/
void destroy_threadpool(threadpool* destroyme)
{
		if(destroyme == NULL)
		{
				perror("\narguments failure\n");
				return;
		}
		//////////////////////////////////////////////////////////////////////////
		////--------------------free and set all clear------------------------////
		//////////////////////////////////////////////////////////////////////////

		if(pthread_mutex_lock(&(destroyme->qlock)) != 0)
		{
				perror("\nmutex failure\n");
				return;
		}

		/////-------Set don’t_accept flag to 1-----/////
		destroyme->dont_accept = 1;

		/////-----Wait for queue to become empty----/////
		if(destroyme->qsize != 0)
		{
			if(pthread_cond_wait(&(destroyme->q_not_empty), &(destroyme->qlock)) != 0)
			{
					perror("\nmutex failure\n");
					return;
			}
		}


		/////------- Set shutdown flag to 1--------/////
		destroyme->shutdown = 1;

		/////---------Signal threads---------------/////
		if(pthread_cond_broadcast(&(destroyme->q_empty)) != 0)
		{
				perror("\nbroadcast failure\n");
				return;
		}

		///////////////////////////////////////////////////////////////////////
		////-------------------join all the threads------------------------////
		///////////////////////////////////////////////////////////////////////
		if(pthread_mutex_unlock(&(destroyme->qlock)) != 0)
		{
				perror("\nmutex failure\n");
				return;
		}
		/////---------Join all threads-------------/////
		int i;
		for (i = 0; i < destroyme->num_threads; i++)
		{
				int check  =0;
				check = pthread_join(destroyme->threads[i], NULL);
				if( check != 0 )
				{
						perror("\njoins failure\n");
						return;
				}
		}

		///////////////////////////////////////////////////////////////////////
		////--------------Free whatever you have to free------------------/////
		///////////////////////////////////////////////////////////////////////

		if(pthread_mutex_destroy(&(destroyme->qlock)) != 0)
		{
				perror("\nmutex failure\n");
				return;
		}

		if(pthread_cond_destroy(&(destroyme->q_empty)) != 0)
		{
				perror("\ncondition failure\n");
				return;
		}
		if(pthread_cond_destroy(&(destroyme->q_not_empty)) != 0)
		{
				perror("\ncond failure\n");
				return;
		}
		free(destroyme->threads);
		free(destroyme);
}
