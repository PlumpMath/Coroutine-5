#include <cassert>
#include <stdexcept>
#include <iostream>
#include <string>
#include "Coroutine.h"


using std::cerr;
using std::endl; // for test

unsigned int Coroutine::sid_ = 0;
Coroutine    Coroutine::main_;
Coroutine*   Coroutine::current_ = 0;


Coroutine::Coroutine(const Function& func, std::size_t  size) : id_( ++ sid_),
                                                                state_(State_init)
                                                            #if defined(__gnu_linux__)
                                                                ,stack_(size ? size : kDefaultStackSize)
                                                            #endif
{
    if (this == &main_)
    {
#if defined(__gnu_linux__)
        std::vector<char>().swap(stack_);
#endif
        return;
    }

    if (id_ == main_.id_)
        id_ = ++ sid_;  // when sid_ overflow

#if defined(__gnu_linux__)
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

    func_  = func;
}

Coroutine::~Coroutine()
{
    cerr << "delete coroutine " << id_ << endl;

#if !defined(__gnu_linux__)
    if (handle_ != INVALID_HANDLE_VALUE)
    {
        ::DeleteFiber(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }
#endif
}

std::shared_ptr<void> Coroutine::_Send(Coroutine* pCrt, std::shared_ptr<void> param)
{
    if (!pCrt)  return 0;

    assert(this == current_);
    assert(this != pCrt);

    current_ = pCrt;

    if (param) {
        this->outputParams_ = param; // set old coroutine's out param
        pCrt->inputParams_ = param;
    }

#if defined(__gnu_linux__)
    int ret = ::swapcontext(&handle_, &pCrt->handle_);
    if (ret != 0) {
        perror("FATAL ERROR: swapcontext");
    }

#else
    ::SwitchToFiber(pCrt->handle_);

#endif

    return  pCrt->outputParams_;
}

std::shared_ptr<void> Coroutine::_Yield(const std::shared_ptr<void>& param)
{
    return _Send(&main_, param);
}

void Coroutine::_Run(Coroutine* pCrt)
{
    assert (&Coroutine::main_ != pCrt);
    assert (Coroutine::current_ == pCrt);

    cerr << "\n=========== Start croutine id "
         << pCrt->GetID() << endl;

    pCrt->state_ = State_running;

    if (pCrt->func_)
        pCrt->func_(pCrt->inputParams_, pCrt->outputParams_);

    cerr << "=========== Finish croutine id "
         << pCrt->GetID() << endl << endl;

    pCrt->inputParams_.reset();
    pCrt->state_ = State_finish;
    pCrt->_Yield(pCrt->outputParams_);
}

CoroutinePtr  CoroutineMgr::CreateCoroutine(const Coroutine::Function& func) 
{
    return  CoroutinePtr(new Coroutine(func)); 
}

CoroutinePtr  CoroutineMgr::_FindCoroutine(unsigned int id) const
{
    CoroutineMap::const_iterator  it(coroutines_.find(id));

    if (it != coroutines_.end())
        return  it->second;

    return  CoroutinePtr();
}

std::shared_ptr<void>  CoroutineMgr::Send(unsigned int id, std::shared_ptr<void> param)
{
    assert (id != Coroutine::main_.id_);
    return  Send(_FindCoroutine(id), param);
}

std::shared_ptr<void>  CoroutineMgr::Send(const CoroutinePtr& pCrt, std::shared_ptr<void> param)
{
    if (pCrt->state_ == Coroutine::State_finish) {
        throw std::runtime_error("Send to a finished coroutine.");
    }

    if (!Coroutine::current_)
    {
        Coroutine::current_ = &Coroutine::main_;

#if !defined(__gnu_linux__)
        Coroutine::main_.handle_ = ::ConvertThreadToFiberEx(&Coroutine::main_, FIBER_FLAG_FLOAT_SWITCH);

#endif
    }

    return  Coroutine::current_->_Send(pCrt.get(), param);
}

std::shared_ptr<void> CoroutineMgr::Yield(const std::shared_ptr<void>& param)
{
    return Coroutine::current_->_Yield(param);
}


CoroutineMgr::~CoroutineMgr()
{
#if !defined(__gnu_linux__)
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

