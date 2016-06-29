#include <cassert>
#include <stdexcept>
#include <iostream>
#include <string>
#include "Coroutine.h"


using std::cerr;
using std::endl; // for test

unsigned int Coroutine::s_id = 0;
Coroutine    Coroutine::s_main;
Coroutine*   Coroutine::s_current = 0;


Coroutine::Coroutine(const Function& func, std::size_t  size) : m_id( ++ s_id),
                                                                m_state(State_init)
                                                            #if defined(__gnu_linux__)
                                                                ,m_stack(size ? size : kStackSize)
                                                            #endif
{
    if (this == &s_main)
    {
#if defined(__gnu_linux__)
        std::vector<char>().swap(m_stack);
#endif
        return;
    }

    if (m_id == s_main.m_id)
        m_id = ++ s_id;  // when s_id overflow

#if defined(__gnu_linux__)
    int ret = ::getcontext(&m_handle);
    assert (ret == 0);

    m_handle.uc_stack.ss_sp   = &m_stack[0];
    m_handle.uc_stack.ss_size = m_stack.size();
    m_handle.uc_link = 0;

    ::makecontext(&m_handle, reinterpret_cast<void (*)(void)>(&Coroutine::_Run), 1, this);

#else
    m_handle = ::CreateFiberEx(0, 0, FIBER_FLAG_FLOAT_SWITCH,
                               reinterpret_cast<PFIBER_START_ROUTINE>(&Coroutine::_Run), this);
#endif

    m_func  = func;
}

Coroutine::~Coroutine()
{
    cerr << "delete coroutine " << m_id << endl;

#if !defined(__gnu_linux__)
    if (m_handle != INVALID_HANDLE_VALUE)
    {
        ::DeleteFiber(m_handle);
        m_handle = INVALID_HANDLE_VALUE;
    }
#endif
}

std::shared_ptr<void> Coroutine::_Send(Coroutine* pCrt, std::shared_ptr<void> param)
{
    if (!pCrt)  return 0;

    assert(this == s_current);
    assert(this != pCrt);

    s_current = pCrt;

    if (param) {
        this->m_outParams = param; // set old coroutine's out param
        pCrt->m_inParams = param;
    }

#if defined(__gnu_linux__)
    int ret = ::swapcontext(&m_handle, &pCrt->m_handle);
    if (ret != 0) {
        perror("FATAL ERROR: swapcontext");
    }

#else
    ::SendFiber(pCrt->m_handle);

#endif

    return  pCrt->m_outParams;
}

std::shared_ptr<void> Coroutine::_Yield(const std::shared_ptr<void>& param)
{
    return _Send(&s_main, param);
}

void Coroutine::_Run(Coroutine* pCrt)
{
    assert (&Coroutine::s_main != pCrt);
    assert (Coroutine::s_current == pCrt);

    cerr << "\n=========== Start croutine id "
         << pCrt->GetID() << endl;

    pCrt->m_state = State_running;

    if (pCrt->m_func)
        pCrt->m_func(pCrt->m_inParams, pCrt->m_outParams);

    cerr << "=========== Finish croutine id "
         << pCrt->GetID() << endl << endl;

    pCrt->m_inParams.reset();
    pCrt->m_state = State_finish;
    pCrt->_Yield(pCrt->m_outParams);
}

CoroutinePtr  CoroutineMgr::CreateCoroutine(const Coroutine::Function& func) 
{
    return  CoroutinePtr(new Coroutine(func)); 
}

CoroutinePtr  CoroutineMgr::_FindCoroutine(unsigned int id) const
{
    CoroutineMap::const_iterator  it(m_coroutines.find(id));

    if (it != m_coroutines.end())
        return  it->second;

    return  CoroutinePtr();
}

std::shared_ptr<void>  CoroutineMgr::Send(unsigned int id, std::shared_ptr<void> param)
{
    assert (id != Coroutine::s_main.m_id);
    return  Send(_FindCoroutine(id), param);
}

std::shared_ptr<void>  CoroutineMgr::Send(const CoroutinePtr& pCrt, std::shared_ptr<void> param)
{
    if (pCrt->m_state == Coroutine::State_finish) {
        throw std::runtime_error("Send to a finished coroutine.");
    }

    if (!Coroutine::s_current)
    {
        Coroutine::s_current = &Coroutine::s_main;

#if !defined(__gnu_linux__)
        Coroutine::s_main.m_handle = ::ConvertThreadToFiberEx(&Coroutine::s_main, FIBER_FLAG_FLOAT_SWITCH);

#endif
    }

    return  Coroutine::s_current->_Send(pCrt.get(), param);
}

std::shared_ptr<void> CoroutineMgr::Yield(const std::shared_ptr<void>& param)
{
    return Coroutine::s_current->_Yield(param);
}


CoroutineMgr::~CoroutineMgr()
{
#if !defined(__gnu_linux__)
    if (::GetCurrentFiber() == Coroutine::s_main.m_handle)
    {
        cerr << "Destroy main fiber\n";
        ::ConvertFiberToThread();
        Coroutine::s_main.m_handle = INVALID_HANDLE_VALUE;
    }
    else
    {
        cerr << "What fucking happened???\n";
    }

#endif
}

