#ifndef BERT_COROUTINE_H
#define BERT_COROUTINE_H

// Only linux & windows
// If you use coroutine with multi threads, you must declare a coroutine mgr for each thread, don't mix!

#if defined(__gnu_linux__)
#include <ucontext.h>

#else   // windows
#include <Windows.h>
#undef    Yield // cancel the windows macro

#endif

#include <vector>
#include <map>
#include <memory>
#include <functional>

class  Coroutine
{
    friend class CoroutineMgr;

    enum State
    {
        State_init,
        State_running,
        State_finish,
    };

public:
    typedef  std::vector<std::shared_ptr<void> >  Params;
    typedef  std::function<void (const Params& inParam, Params& outParam)>  Function;

    ~Coroutine();

    unsigned int    GetID() const  { return  m_id; }
    static unsigned int GetCurrentID()  {  return s_current->m_id; } 

private:
    Coroutine(const Function& func = Function(), std::size_t size = 0);

    Params*         _SwitchTo(Coroutine* pCrt, Params*  inParam = 0);
    void            _Yield();
    static void     _Run(Coroutine* cxt);

    unsigned int    m_id;  // 1:main
    int             m_state;
    Params          m_inParams;
    Params          m_outParams;

#if defined(__gnu_linux__)
    typedef ucontext HANDLE;

    static const std::size_t kStackSize = 8 * 1024; 
    std::vector<char>   m_stack;

#else
    typedef PVOID    HANDLE;

#endif

    HANDLE      m_handle;
    Function    m_func;

    static Coroutine        s_main;
    static Coroutine*       s_current; 
    static unsigned int     s_id;
};

typedef std::shared_ptr<Coroutine>     CoroutinePtr;

class CoroutineMgr
{
public:
    static CoroutinePtr     CreateCoroutine(const Coroutine::Function& func);

    Coroutine::Params*      SwitchTo(unsigned int id, Coroutine::Params*  param = 0);
    Coroutine::Params*      SwitchTo(const CoroutinePtr& pCrt, Coroutine::Params*  param = 0);
    static void             Yield();

    ~CoroutineMgr();

    // TODO process timeout, recycle, etc

private:
    CoroutinePtr  _FindCoroutine(unsigned int id) const;

    typedef std::map<unsigned int, CoroutinePtr >   CoroutineMap;
    CoroutineMap    m_coroutines;
};

#endif

