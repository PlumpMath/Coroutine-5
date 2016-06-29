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
    Coroutine(const Coroutine&) = delete;
    void operator=(const Coroutine&) = delete;

    unsigned int GetID() const  { return  id_; }
    static unsigned int GetCurrentID()  {  return current_->id_; } 

private:
    Coroutine(const Function& func = Function(), std::size_t size = 0);

    std::shared_ptr<void> _Send(Coroutine* pCrt, std::shared_ptr<void> inParam = std::shared_ptr<void>(nullptr));
    std::shared_ptr<void> _Yield(const std::shared_ptr<void>& = std::shared_ptr<void>(nullptr));
    static void     _Run(Coroutine* cxt);

    unsigned int    id_;  // 1: main
    int             state_;
    std::shared_ptr<void> inputParams_;
    std::shared_ptr<void> outputParams_;

#if defined(__gnu_linux__)
    typedef ucontext HANDLE;

    static const std::size_t kDefaultStackSize = 8 * 1024; 
    std::vector<char>   stack_;

#else
    typedef PVOID    HANDLE;

#endif

    HANDLE      handle_;
    Function    func_;

    static Coroutine        main_;
    static Coroutine*       current_; 
    static unsigned int     sid_;
};

using CoroutinePtr = std::shared_ptr<Coroutine>;

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

private:
    CoroutinePtr  _FindCoroutine(unsigned int id) const;

    typedef std::map<unsigned int, CoroutinePtr >   CoroutineMap;
    CoroutineMap    coroutines_;
};

#endif

