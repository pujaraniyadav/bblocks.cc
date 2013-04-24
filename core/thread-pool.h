#ifndef _DH_CORE_THREADPOOL_H_
#define _DH_CORE_THREADPOOL_H_

#include "core/thread.h"

namespace dh_core {

#define COMMA ,
#define SEMICOLON ;
#define OPENBRACKET (
#define CLOSEBRACKET )

class NonBlockingThread;

/**
 *
 */
class ThreadRoutine : public InListElement<ThreadRoutine>
{
public:

    virtual void Run() = 0;
    virtual ~ThreadRoutine() {}
};

/**
 *
 */
#define MEMBERFNPTR(A, B, C, D, E, F, G) \
template<class _OBJ_, B> \
class MemberFnPtr##A : public ThreadRoutine \
{ \
public: \
\
    MemberFnPtr##A(_OBJ_ * obj, void (_OBJ_::*fn)(C), D) \
        : obj_(obj), fn_(fn) \
    { \
        E; \
    } \
\
    virtual void Run() \
    { \
        (obj_->*fn_)(F); \
        delete this; \
    } \
\
private: \
\
    _OBJ_ * obj_; \
    void (_OBJ_::*fn_)(C); \
    G; \
}\
ALIGNED(sizeof(uint64_t));

MEMBERFNPTR(, class _P_, _P_, const _P_ p, p_ = p, p_, _P_ p_)

MEMBERFNPTR(2, class _P1_ COMMA class _P2_, _P1_ COMMA _P2_,
            const _P1_ p1 COMMA const _P2_ p2,
            p1_ = p1 SEMICOLON p2_ = p2, p1_ COMMA p2_,
            _P1_ p1_ SEMICOLON _P2_ p2_)

MEMBERFNPTR(3,
            class _P1_ COMMA class _P2_ COMMA class _P3_,
            _P1_ COMMA _P2_ COMMA _P3_,
            const _P1_ p1 COMMA const _P2_ p2 COMMA const _P3_ p3,
            p1_ = p1 SEMICOLON p2_ = p2 SEMICOLON p3_ = p3,
            p1_ COMMA p2_ COMMA p3_,
            _P1_ p1_ SEMICOLON _P2_ p2_ SEMICOLON _P3_ p3_)


/**
 *
 */
class NonBlockingLogic
{
public:

    friend class NonBlockingThreadPool;

    NonBlockingLogic()
        : thAffinity_(RRCpuId::Instance().GetId())
    {
    }

    virtual ~NonBlockingLogic()
    {
    }

protected:

    const uint32_t thAffinity_;
};

/**
 *
 */
class NonBlockingThread : public Thread
{
public:

    NonBlockingThread(const std::string & path)
        : Thread(path)
        , q_(path)
    {
    }

    virtual void * ThreadMain()
    {
        while (!exitMain_)
        {
            EnableThreadCancellation();
            ThreadRoutine * r = q_.Pop();
            DisableThreadCancellation();

            r->Run();
        }

        return NULL;
    }

    void Push(ThreadRoutine * r)
    {
        q_.Push(r);
    }

private:

    InQueue<ThreadRoutine> q_;
};

/**
 *
 */
class NonBlockingThreadPool : public Singleton<NonBlockingThreadPool>
{
public:

    NonBlockingThreadPool()
        : nextTh_(0)
    {
    }

    void Start(const uint32_t maxCores)
    {
        AutoLock _(&lock_);

        for (size_t i = 0; i < maxCores; ++i) {
            NonBlockingThread * th = new NonBlockingThread("/th/" + STR(i));
            threads_.push_back(th);
            th->StartThread();
        }
    }

    void Shutdown()
    {
        AutoLock _(&lock_);
        DestroyThreads();
        condExit_.Broadcast();
    }

    void Wait()
    {
        AutoLock _(&lock_);
        condExit_.Wait(&lock_);
    }

#define NBTP_SCHEDULE(A, B, C, D, E) \
    template<class _OBJ_, A> \
    void Schedule(_OBJ_ * obj, void (_OBJ_::*fn)(B), C) \
    { \
        ThreadRoutine * r = new MemberFnPtr##E<_OBJ_, B>(obj, fn, D); \
        threads_[nextTh_++ % threads_.size()]->Push(r); \
    }

    NBTP_SCHEDULE(class _P_, _P_, const _P_ p, p, )
    NBTP_SCHEDULE(class _P1_ COMMA class _P2_, _P1_ COMMA _P2_,
                  const _P1_ p1 COMMA const _P2_ p2, p1 COMMA p2, 2)
    NBTP_SCHEDULE(class _P1_ COMMA class _P2_ COMMA class _P3_,
                  _P1_ COMMA _P2_ COMMA _P3_,
                  const _P1_ p1 COMMA const _P2_ p2 COMMA const _P3_ p3,
                  p1 COMMA p2 COMMA p3, 3)


    void Schedule(ThreadRoutine * r)
    {
        threads_[nextTh_++ % threads_.size()]->Push(r);
    }

private:

    typedef std::vector<NonBlockingThread *> threads_t;

    void DestroyThreads()
    {
        for (threads_t::iterator it = threads_.begin(); it != threads_.end();
             ++it) {
            NonBlockingThread * th = *it;
            // Stop the thread
            th->Stop();
            // destroy the thread object
            delete th;
        }

        threads_.clear();
    }

    PThreadMutex lock_;
    threads_t threads_;
    WaitCondition condExit_;
    uint32_t nextTh_;
};

class ThreadPool
{
public:

    static void Start(const uint32_t ncores)
    {
        NonBlockingThreadPool::Instance().Start(ncores);
    }

    static void Shutdown()
    {
        NonBlockingThreadPool::Instance().Shutdown();
    }

    static void Wait()
    {
        NonBlockingThreadPool::Instance().Wait();
    }

#define SCHEDULE(A, B, C, D) \
    template<class _OBJ_, A> \
    static void Schedule(_OBJ_ * obj, void (_OBJ_::*fn)(B), C) \
    { \
        NonBlockingThreadPool::Instance().Schedule(obj, fn, D); \
    }

    SCHEDULE(class _P_, _P_, const _P_ p, p)

    SCHEDULE(class _P1_ COMMA class _P2_,
             _P1_ COMMA _P2_,
             const _P1_ p1 COMMA const _P2_ p2,
             p1 COMMA p2)

    SCHEDULE(class _P1_ COMMA class _P2_ COMMA class _P3_,
             _P1_ COMMA _P2_ COMMA _P3_,
             const _P1_ p1 COMMA const _P2_ p2 COMMA const _P3_ p3,
             p1 COMMA p2 COMMA p3)


    static void Schedule(ThreadRoutine * r)
    {
        NonBlockingThreadPool::Instance().Schedule(r);
    }
};

} // namespace dh_core

#endif