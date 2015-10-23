#include <cassert>
#include <iostream>
#include "Coroutine.h"


using namespace std; // for test

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
        cerr << "No need to construct Main\n";
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

Coroutine::Params*  Coroutine::_SwitchTo(Coroutine* pCrt, Coroutine::Params* param)
{
    if (!pCrt)  return 0;

    assert(this == s_current);
    assert(this != pCrt);

    s_current = pCrt;

    if (param) {
        pCrt->m_inParams.swap(*param);
        param->clear();
    }

#if defined(__gnu_linux__)
    int ret = ::swapcontext(&m_handle, &pCrt->m_handle);
    if (ret != 0) {
        perror("FATAL ERROR: swapcontext");
    }

#else
    ::SwitchToFiber(pCrt->m_handle);

#endif

    return  &pCrt->m_outParams;
}

void  Coroutine::_Yield()
{
    _SwitchTo(&s_main, 0);
}

void   Coroutine::_Run(Coroutine* pCrt)
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

    pCrt->m_inParams.clear();
    pCrt->m_state = State_finish;
    pCrt->_Yield();
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

Coroutine::Params*  CoroutineMgr::SwitchTo(unsigned int id, Coroutine::Params*  param)
{
    assert (id != Coroutine::s_main.m_id);
    return  SwitchTo(_FindCoroutine(id), param);
}

Coroutine::Params*  CoroutineMgr::SwitchTo(const CoroutinePtr& pCrt, Coroutine::Params*  param)
{
    if (pCrt->m_state == Coroutine::State_finish)
        return 0;

    if (!Coroutine::s_current)
    {
        cerr << "Init s_current\n";
        Coroutine::s_current = &Coroutine::s_main;

#if !defined(__gnu_linux__)
        Coroutine::s_main.m_handle = ::ConvertThreadToFiberEx(&Coroutine::s_main, FIBER_FLAG_FLOAT_SWITCH);

#endif
    }

    return  Coroutine::s_current->_SwitchTo(pCrt.get(), param);
}

void CoroutineMgr::Yield()
{
    Coroutine::s_current->_Yield();
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



////////////////////////////////////
// test function for coroutine
////////////////////////////////////

void  Run1(const Coroutine::Params& inParams, Coroutine::Params& outParams)
{
    cerr << Coroutine::GetCurrentID() << " Enter Run1\n" ;

    for (Coroutine::Params::const_iterator it(inParams.begin());
        it != inParams.end();
        ++ it)
    {
        cerr << Coroutine::GetCurrentID()
            << " input params "
            << *(int*)it->get()
            << endl;
    }

    outParams.push_back(shared_ptr<int>(new int(1)));

    cerr << Coroutine::GetCurrentID() << " From Run1, return to Main\n";
    CoroutineMgr::Yield();

    cerr << Coroutine::GetCurrentID() << " Resume Run1, from Main\n";
    for (Coroutine::Params::const_iterator it(inParams.begin());
        it != inParams.end();
        ++ it)
    {
        cerr << Coroutine::GetCurrentID()
            << " resume params "
            << *(int*)it->get()
            << endl;
    }

    cerr << Coroutine::GetCurrentID() << " Exit Run1\n";
}

void  Run2(const Coroutine::Params& inParams, Coroutine::Params& outParam)
{
    cerr << Coroutine::GetCurrentID() << " Enter Run2\n" ;

    for (Coroutine::Params::const_iterator it(inParams.begin());
        it != inParams.end();
        ++ it)
    {
        cerr << Coroutine::GetCurrentID()
            << " input params "
            << *(int*)it->get()
            << endl;
    }

    outParam.push_back(shared_ptr<int>(new int(2)));

    cerr << Coroutine::GetCurrentID() << " Exit Run2\n";
}


int main()
{
    cerr << " begin main\n";

    //0. define CoroutineMgr object for each thread.
    CoroutineMgr   mgr;

    //1. create coroutine
    CoroutinePtr  crt1(mgr.CreateCoroutine(Run1));
    CoroutinePtr  crt2(mgr.CreateCoroutine(Run2));

    //2. prepare params
    Coroutine::Params  params;
    params.push_back(shared_ptr<int>(new int(-1)));
    params.push_back(shared_ptr<int>(new int(-2)));

    //3. jump to crt1, get result from crt1
    Coroutine::Params* pRet = mgr.SwitchTo(crt1, &params);

    //4. jump to crt2, pass crt1's result
    pRet = mgr.SwitchTo(crt2, pRet);

    //5. return to crt1, pass crt2's result
    (void)mgr.SwitchTo(crt1, pRet);

    cerr << "BYE BYE\n";
}

