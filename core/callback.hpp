#ifndef _DHCORE_CALLBACK_H_
#define _DHCORE_CALLBACK_H_

#include "core/thread-pool.h"

namespace dh_core {

#define CALLBACK(A, B, C, D, E, F, G) \
template<B> \
class A : public ThreadRoutine \
{ \
public: \
\
    virtual void ScheduleCallback(C) = 0; \
}; \
\
template<class _OBJ_, B> \
class MemberFn##A : public A<C> \
{ \
public: \
\
    MemberFn##A(_OBJ_ * obj, void (_OBJ_::*fn)(C)) \
        : obj_(obj), fn_(fn) \
    {} \
\
    virtual void ScheduleCallback(D) \
    { \
        E; \
        NonBlockingThreadPool::Instance().Schedule(this); \
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
}; \
\
template<class _OBJ_, B> \
A<C> * \
make_cb(_OBJ_ * obj, void (_OBJ_::*fn)(C)) \
{ \
    return new MemberFn##A<_OBJ_, C>(obj, fn); \
}

CALLBACK(Callback, class _P_, _P_, _P_ p, p_ = p, p_, _P_ p_)
CALLBACK(Callback2, class _P1_ COMMA class _P2_, _P1_ COMMA _P2_,
         _P1_ p1 COMMA _P2_ p2, p1_ = p1 SEMICOLON p2_ = p2, p1_ COMMA p2_,
         _P1_ p1_ SEMICOLON _P2_ p2_)


} // namespace dh_core

#endif
