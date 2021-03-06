/*
 * Copyright (c) 2010 Yahoo! Inc. All rights reserved. Licensed under
 * the Apache License, Version 2.0 (the "License"); you may not use
 * this file except in compliance with the License. You may obtain a
 * copy of the License at http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * permissions and limitations under the License. See accompanying
 * LICENSE file.
 * 
 * $Id$
 */

#ifndef _CL_THREAD_H_
#define _CL_THREAD_H_

#include <errno.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

namespace clusterlib {

class Thread
{
  public:
    typedef void* (*ThreadFunc) (void*);

    /**
     * Constructor.
     */
    Thread() 
        : _ctx(NULL), 
          _func(NULL) 
    {
        memset(&mThread, 0, sizeof(mThread));
    }

    /**
     * Destructor.
     */
    ~Thread() { }

    /** 
     * Create the thread with the appropriate function.
     *
     * @param ctx the context passed into pthread_create
     * @param func the function for the thread to run
     */
    void Create(void* ctx, ThreadFunc func);

    /**
     * Wait for this thread to end.
     */ 
    void Join();

  private:
    /**
     * Do not copy construct 
     */
    Thread(const Thread &);

    /**
     * Do not assign.
     */
    Thread & operator= (const Thread &);

  private:
    pthread_t mThread;  
    void *_ctx;
    ThreadFunc _func;
};

template<typename T>
struct ThreadContext
{
    typedef void (T::*FuncPtr) (void *);
    ThreadContext(T& ctx, FuncPtr func, void *funcParam) 
        : _ctx(ctx), _func(func), _funcParam(funcParam) {}
    void run(void) 
    {
        (_ctx.*_func)(_funcParam);
    }
    T& _ctx;
    FuncPtr _func;
    void *_funcParam;
};

template<typename T>
void* ThreadExec(void *obj)
{
    ThreadContext<T>* tc = (ThreadContext<T>*)(obj);
    assert(tc != 0);
    tc->run();
    return 0;
}

/**
 * Allows a thread to call a C++ member function of a class.
 */
template <typename T>
class CXXThread
    : public Thread
{
  public:
    typedef void (T::*FuncPtr) (void *);
    CXXThread() : ctx(0) {}
    ~CXXThread() { if (ctx) delete ctx; }

    void Create(T& obj, FuncPtr func, void *funcParam = NULL) 
    {
        assert(ctx == 0);
        ctx = new ThreadContext<T>(obj, func, funcParam);
        Thread::Create(ctx, ThreadExec<T>);
    }

    /**
     * Get the internal PredMutexCond for signaling this object's
     * thread to stop.
     *
     * @return Reference to internal m_predMutexCond
     */
    PredMutexCond & getPredMutexCond()
    {
        return m_predMutexCond;
    }

  private:
    /**
     * Used to get the context to call its member function.
     */
    ThreadContext<T>* ctx;

    /**
     * Used to signal the thread to exit.
     */
    PredMutexCond m_predMutexCond;
};

}	/* End of 'namespace clusterlib' */

#endif /* _CL_THREAD_H_ */

