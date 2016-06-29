#include <cassert>
#include <iostream>
#include <string>
#include "Coroutine.h"


using std::cerr;
using std::endl; // for test


// A coroutine: simply twice the input integer and return
void Double(const std::shared_ptr<void>& inParams, std::shared_ptr<void>& outParams)
{
    int input = *std::static_pointer_cast<int>(inParams);
    cerr << "Coroutine Double: got input params "
         << input
         << endl;

    cerr << "Coroutine Double: Return to Main." << endl;
    auto rsp = CoroutineMgr::Yield(std::make_shared<std::string>("I am calculating, please wait...")); 

    cerr << "Coroutine Double is resumed from Main\n"; 

    cerr<< "Coroutine Double: got message \"" 
        << *std::static_pointer_cast<std::string>(rsp) << "\""
        << endl; 

    // double the input
    outParams = std::make_shared<int>(input * 2);

    cerr << "Exit " << __FUNCTION__ << endl;
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
        
    cerr << "BYE BYE\n";
}
