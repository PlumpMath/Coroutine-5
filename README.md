#Coroutine
=========

A python like coroutine implementation in C++11.

## Requirements
* C++11
* Linux or Windows

## Principle
* linux: Use `swapcontext`, please `man makecontext`.
* windows: the fiber API. See [MSDN](https://msdn.microsoft.com/en-us/library/windows/desktop/ms682661(v=vs.85).aspx) for details.

## Code example
```c++

// A coroutine: simply twice the input integer and return
void Double(const std::shared_ptr<void>& inParams, std::shared_ptr<void>& outParams)
{
    int input = *std::static_pointer_cast<int>(inParams);
    
    // yield to main.
    auto rsp = CoroutineMgr::Yield(std::make_shared<std::string>("I am calculating, please wait...")); 

    // resumed from main, print message.
    cerr<< "Coroutine Double: got message \"" 
        << *std::static_pointer_cast<std::string>(rsp) << "\""
        << endl; 

    // calculate the result. (double the input and return)
    // the outParams will be the return value of Send().
    outParams = std::make_shared<int>(input * 2);
}


int main()
{
    //0. define CoroutineMgr object for each thread.  
    CoroutineMgr   mgr;
    //1. create coroutine
    CoroutinePtr  crt(mgr.CreateCoroutine(Double));
        
    //2. jump to crt1, get result from crt1
    const int input = 42;
    auto pRet = mgr.Send(crt, std::make_shared<int>(input));
    cerr << "Main func: got reply message \"" << *std::static_pointer_cast<std::string>(pRet).get() << "\""<< endl;

    //3. got the final result: 84
    auto finalResult = mgr.Send(crt, std::make_shared<std::string>("Please be quick, I am waiting for your result"));
    cerr << "Main func: got the twice of " << input << ", answer is " << *std::static_pointer_cast<int>(finalResult) << endl;

    return 0;
}

```

