#ifndef BERT_COROUTINE_H
#define BERT_COROUTINE_H

// Only linux & windows
// If you use coroutine with multi threads, you must declare a coroutine mgr for each thread

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
    typedef  std::function<void (const std::shared_ptr<void>& inParam, std::shared_ptr<void>& outParam)>  Function;

    ~Coroutine();

    unsigned int GetID() const  { return  m_id; }
    static unsigned int GetCurrentID()  {  return s_current->m_id; } 

private:
    Coroutine(const Function& func = Function(), std::size_t size = 0);

    std::shared_ptr<void> _Send(Coroutine* pCrt, std::shared_ptr<void> inParam = std::shared_ptr<void>(nullptr));
    std::shared_ptr<void> _Yield(const std::shared_ptr<void>& = std::shared_ptr<void>(nullptr));
    static void     _Run(Coroutine* cxt);

    unsigned int    m_id;  // 1: main
    int             m_state;
    std::shared_ptr<void> m_inParams;
    std::shared_ptr<void> m_outParams;

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
    // works like python decorator: convert the func to a coroutine
    static CoroutinePtr     CreateCoroutine(const Coroutine::Function& func);

    // like python generator's send method
    std::shared_ptr<void>   Send(unsigned int id, std::shared_ptr<void> param = std::shared_ptr<void>(nullptr));
    std::shared_ptr<void>   Send(const CoroutinePtr& pCrt, std::shared_ptr<void> param = std::shared_ptr<void>(nullptr));
    static std::shared_ptr<void> Yield(const std::shared_ptr<void>& = std::shared_ptr<void>(nullptr));

    ~CoroutineMgr();

    // TODO process timeout, recycle, etc

private:
    CoroutinePtr  _FindCoroutine(unsigned int id) const;

    typedef std::map<unsigned int, CoroutinePtr >   CoroutineMap;
    CoroutineMap    m_coroutines;
};

#endif

