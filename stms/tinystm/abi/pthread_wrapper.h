/*
 * File:
 *   pthread_wrapper.h
 * Author(s):
 *   Pascal Felber <pascal.felber@unine.ch>
 *   Patrick Marlier <patrick.marlier@unine.ch>
 * Description:
 *   Pthread wrapper to handle thread creation.
 *
 * Copyright (c) 2007-2014.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * This program has a dual license and can also be distributed
 * under the terms of the MIT license.
 */

#include <dlfcn.h> 

/* Original pthread function */
static int (*pthread_create_orig)(pthread_t *__restrict,
                                  __const pthread_attr_t *__restrict,
                                  void *(*)(void *),
                                  void *__restrict) = NULL; 

typedef struct {
  void * (*start_routine)(void *);
  void * arg;
} wrapper_t; 

static void * wpthread_create(void * data)
{
  void * (*start_routine)(void *) = ((wrapper_t*)data)->start_routine;
  void * arg = ((wrapper_t*)data)->arg; 
  void * ret;
  /* Free the allocated memory by the wrapper */
  free(data);
  /* Initialize thread */
  _ITM_initializeThread();
  /* Call user function */
  ret = start_routine(arg);
  /* Finalizing thread */
  _ITM_finalizeThread();
  return ret;
}

int pthread_create(pthread_t *__restrict thread,
                   __const pthread_attr_t *__restrict attr,
                   void * (*start_routine)(void *),
                   void *__restrict arg)
{ 
  int i_return;
  /* Allocate memory to pass as argument (we can't assume that stack will not be modified) */
  wrapper_t * wdata = malloc(sizeof(wrapper_t));
  wdata->start_routine = start_routine;
  wdata->arg = arg;
  /* Solving link to original pthread_create */
  if (pthread_create_orig == NULL) {
    pthread_create_orig = dlsym(RTLD_NEXT, "pthread_create");
    if (pthread_create_orig == NULL) {
      char *error = dlerror();
      if (error == NULL) {
        error = "pthread_create can't be solved.";
      }  
      fprintf(stderr, "%s\n", error);
      exit(EXIT_FAILURE);
    }
  }
  /* Call original pthread function */
  i_return = pthread_create_orig(thread, attr, wpthread_create, wdata);
  return i_return;
}
