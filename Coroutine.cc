#include <cassert>
#include <stdexcept>
#include <iostream>
#include <string>
#include "Coroutine.h"


using std::cerr;
using std::endl; // for test

unsigned int Coroutine::sid_ = 0;
Coroutine Coroutine::main_;
Coroutine* Coroutine::current_ = 0;


Coroutine::Coroutine(std::size_t size) :
    id_( ++ sid_),
    state_(State_init)
#if defined(__gnu_linux__) || defined(__APPLE__)
    ,stack_(size > kDefaultStackSize ? size : kDefaultStackSize)
#endif
{
    if (this == &main_)
    {
#if defined(__gnu_linux__) || defined(__APPLE__)
        std::vector<char>().swap(stack_);
#endif
        return;
    }

    if (id_ == main_.id_)
        id_ = ++ sid_;  // when sid_ overflow

#if defined(__gnu_linux__) || defined(__APPLE__)
    int ret = ::getcontext(&handle_);
    assert (ret == 0);

    handle_.uc_stack.ss_sp   = &stack_[0];
    handle_.uc_stack.ss_size = stack_.size();
    handle_.uc_link = 0;

    ::makecontext(&handle_, reinterpret_cast<void (*)(void)>(&Coroutine::_Run), 1, this);

#else
    handle_ = ::CreateFiberEx(0, 0, FIBER_FLAG_FLOAT_SWITCH,
                               reinterpret_cast<PFIBER_START_ROUTINE>(&Coroutine::_Run), this);
#endif
}

Coroutine::~Coroutine()
{
    cerr << "delete coroutine " << id_ << endl;

#if !defined(__gnu_linux__) && !defined(__APPLE__)
    if (handle_ != INVALID_HANDLE_VALUE)
    {
        ::DeleteFiber(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }
#endif
}

AnyPointer Coroutine::_Send(Coroutine* crt, AnyPointer param)
{
    assert (crt);

    assert(this == current_);
    assert(this != crt);

    current_ = crt;

    if (param)
    {
        // just behave like python's generator
        if (crt->state_ == State_init)
            throw std::runtime_error("Can't send non-void value to a just-created coroutine");

        // set old coroutine's yield value
        this->yieldValue = std::move(param);
    }

#if defined(__gnu_linux__) || defined(__APPLE__)
    int ret = ::swapcontext(&handle_, &crt->handle_);
    if (ret != 0)
    {
        perror("FATAL ERROR: swapcontext");
        throw std::runtime_error("FATAL ERROR: swapcontext failed");
    }

#else
    ::SwitchToFiber(crt->handle_);

#endif

    return crt->yieldValue;
}

AnyPointer Coroutine::_Yield(const AnyPointer& param)
{
    return _Send(&main_, param);
}

void Coroutine::_Run(Coroutine* crt)
{
    assert (&Coroutine::main_ != crt);
    assert (Coroutine::current_ == crt);

    cerr << "\n=========== Start croutine id "
         << crt->GetID() << endl;

    crt->state_ = State_running;

    if (crt->func_)
        crt->func_();

    cerr << "=========== Finish croutine id "
         << crt->GetID() << endl << endl;

    crt->state_ = State_finish;
    crt->_Yield(crt->result_);
}


CoroutinePtr  CoroutineMgr::_FindCoroutine(unsigned int id) const
{
    CoroutineMap::const_iterator  it(coroutines_.find(id));

    if (it != coroutines_.end())
        return it->second;

    return CoroutinePtr();
}

AnyPointer CoroutineMgr::Send(unsigned int id, AnyPointer param)
{
    assert (id != Coroutine::main_.id_);
    return Send(_FindCoroutine(id), param);
}

AnyPointer CoroutineMgr::Send(const CoroutinePtr& crt, AnyPointer param)
{
    if (crt->state_ == Coroutine::State_finish) {
        throw std::runtime_error("Send to a finished coroutine.");
    }

    if (!Coroutine::current_)
    {
        Coroutine::current_ = &Coroutine::main_;

#if !defined(__gnu_linux__) && !defined(__APPLE__)
        Coroutine::main_.handle_ = ::ConvertThreadToFiberEx(&Coroutine::main_, FIBER_FLAG_FLOAT_SWITCH);

#endif
    }

    return Coroutine::current_->_Send(crt.get(), param);
}

AnyPointer CoroutineMgr::Yield(const AnyPointer& param)
{
    return Coroutine::current_->_Yield(param);
}


CoroutineMgr::~CoroutineMgr()
{
#if !defined(__gnu_linux__) && !defined(__APPLE__)
    if (::GetCurrentFiber() == Coroutine::main_.handle_)
    {
        cerr << "Destroy main fiber\n";
        ::ConvertFiberToThread();
        Coroutine::main_.handle_ = INVALID_HANDLE_VALUE;
    }
    else
    {
        cerr << "What fucking happened???\n";
    }

#endif
}

