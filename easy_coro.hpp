/*
    基于c++20新特性，协程所写的库，库中以类模板的方式封装了c++原生协程的具体
实现，以供方便使用。

    协程类模板中有五个模板参数:
        Tco_return
        Tco_yield
        T_initial_await
        T_final_await
        T_yield_await

        其中，前两个模板参数分别为协程的 co_return 类型和 co_yield 类型。
        后面三个模板参数则应满足一定约定（在本库中用 SatisfyAwait 约束），
    这三个模板参数用于判定在指定条件下是否要挂起协程。
        
    协程类模板已针对前两个模板参数做了相应偏特化处理（针对 void 类型）。
    存放 co_return 或 co_yield 的值时，以指针形式存储。

    若要扩展协程类，则应在自定义类型中，定义一个名为 promise_type 的类型，
该 promise_type 类型应当继承自 BasicCoroutine<>::promise_type 类型。接
下来有两种选择：一、为自定义类型添加由 BasicCoroutine<> 进行转换的隐式转换
函数（自定义类型中应当有相应的成员来存放 BasicCoroutine<> 中的协程句柄）；
二、覆盖 get_return_object 方法，覆盖后的返回值应为自定义类型。准备工作完成
后，即可开始重写相应的虚方法了，所有虚方法的签名已在下面进行了宏定义，可以方
便地重写。这些提供的虚方法会在相应时机被特定的方法调用，以实现特殊需求。

    为了方便地管理协程句柄，本库中根据共享指针的机制提供了共享协程句柄，SharedCoroHandle
类，以便程序自动管理协程的销毁。创建时建议通过 MakeSharedCoroHandle 函数创建。

*/
#pragma once
#if _MSVC_LANG > 201703L
#include<coroutine>
#include<type_traits>
#include<malloc.h>

#define _AWAIT std::suspend_always()
#define _CONTINUE std::suspend_never()

#define CO_AWAIT co_await _AWAIT
#define CO_CONTINUE co_await _CONTINUE

#define OVERRIDE_ON_RETURN_VOID virtual void OnReturn()override
#define OVERRIDE_ON_RETURN virtual void OnReturn(const T_return_type& value)override
#define OVERRIDE_ON_YIELD virtual void OnYield(const T_yield_type& value)override
#define OVERRIDE_ON_GET_OBJ virtual void OnGetObject()override
#define OVERRIDE_ON_INITIAL virtual void OnInitial()override
#define OVERRIDE_ON_FINAL virtual void OnFinal()noexcept override
#define OVERRIDE_ON_UNHANDLED_EXC virtual void OnUnhandledException()override


namespace MyCodes
{
template<class T>
concept SatisfyAwait = requires(T t,bool resault)
{
resault = t.await_ready();
&T::await_suspend;
t.await_resume();
};

template<   class Tco_return,
        class Tco_yield,
        class T_initial_await,
        class T_final_await,
        class T_yield_await>
requires    SatisfyAwait<T_initial_await>&&
            SatisfyAwait<T_final_await>&&
            SatisfyAwait<T_yield_await>
class BasicCoroutine;       //前置声明

template<   class Tco_return,
        class Tco_yield,
        class T_initial_await,
        class T_final_await,
        class T_yield_await>
requires    SatisfyAwait<T_initial_await>&&
SatisfyAwait<T_final_await>&&
SatisfyAwait<T_yield_await>
struct promise_type_base
{
    promise_type_base() {}

    using CoroType =
        BasicCoroutine<Tco_return, Tco_yield, T_initial_await, T_final_await, T_yield_await>;

    CoroType get_return_object()
    {
        this->OnGetObject();
        #pragma warning(disable:6255)
        CoroType* coro_ptr = (CoroType*)_alloca(sizeof(CoroType));  //防止自动调用析构函数
        #pragma warning(default:6255)
        coro_ptr->m_handle = std::coroutine_handle<typename CoroType::promise_type>::
            from_promise(*
                (reinterpret_cast<typename CoroType::promise_type*>(this))
            );

        return *coro_ptr;
    }

    T_initial_await initial_suspend()
    {
        this->OnInitial();
        return {};
    }

    T_final_await final_suspend()noexcept
    {
        this->OnFinal();
        return {};
    }

    void unhandled_exception()
    {
        this->OnUnhandledException();
    }

    virtual void OnGetObject()
    {

    }

    virtual void OnInitial()
    {

    }

    virtual void OnFinal()noexcept
    {

    }

    virtual void OnUnhandledException()
    {

    }
};

template<class CoroutineType>
class SharedCoroHandle;     //前置声明
}

namespace MyCodes
{
//协程模板,Tco_return是co_return的返回值类型,
//Tco_yield是co_yield的返回类型
//T_pt的三个模板参数是满足await_ready条件的类型，用来判断各种情况下是否挂起协程
//默认为std::suspend_always,即总是在这些时候挂起
template<   class Tco_return    = void,
        class Tco_yield     = void,
        class T_initial_await  = std::suspend_always,
        class T_final_await    = std::suspend_always,
        class T_yield_await    = std::suspend_always>
requires    SatisfyAwait<T_initial_await>&&
            SatisfyAwait<T_final_await>&&
            SatisfyAwait<T_yield_await>
class BasicCoroutine
{
public:
    
    using T_yield_type = Tco_yield;
    using T_return_type = Tco_return;
    
    struct promise_type : promise_type_base<Tco_return, Tco_yield, T_initial_await, T_final_await, T_yield_await>
{
    using Base = promise_type;

    promise_type() {}

    ~promise_type()
    {
        delete m_return_value;
        delete m_yield_value;
    }

    void return_value(const T_return_type& value)
    {
        this->OnReturn(value);
    }

    T_yield_await yield_value(const T_yield_type& value)
    {
        this->OnYield(value);
        return {};
    }

    virtual void OnReturn(const T_return_type& value)
    {
        m_return_value = new T_return_type(value);
    }

    virtual void OnYield(const T_yield_type& value)
    {
        if (m_yield_value)
        {
            *m_yield_value = value;
        }
        else
        {
            m_yield_value = new T_yield_type(value);
        }
    }

    _declspec(property(get = getyield)) const T_yield_type* yield_value_ptr;
    const T_yield_type* getyield()const
    {
        return this->m_yield_value;
    }

    _declspec(property(get = getreturn)) const T_return_type* return_value_ptr;
    const T_return_type* getreturn()const
    {
        return this->m_return_value;
    }
private:
    T_yield_type* m_yield_value = nullptr;
    T_return_type* m_return_value = nullptr;

};

    using HandleType = std::coroutine_handle<promise_type>;

private:
    friend class SharedCoroHandle<BasicCoroutine>;
    friend struct promise_type_base<Tco_return, Tco_yield, T_initial_await, T_final_await, T_yield_await>;
    HandleType m_handle;

public:
    
    ~BasicCoroutine()
    {
        this->destory();
    }
    
    bool done()const
    {
        if (m_handle != nullptr)
            return handle.done();
        else
            return true;
    }
    
    void resume()const
    {
        if (m_handle != nullptr)
            handle.resume();
    }
    
    void destory()
    {
        if (m_handle != nullptr)
        {
            this->m_handle.destroy();
            m_handle = nullptr;
        }
    }
    
    bool operator==(const BasicCoroutine& right)const
    {
        return this->handle == right.handle;
    }

    bool operator==(nullptr_t)const
    {
        return m_handle == nullptr;
    }

    _declspec(property(get = gethandle)) const HandleType& handle;
    const HandleType& gethandle()const
    {
        return m_handle;
    }
    
    _declspec(property(get = getyield)) const T_yield_type* yield_value_ptr;
    const T_yield_type* getyield()const
    {
        return this->handle.promise().yield_value_ptr;
    }
    
    _declspec(property(get = getreturn)) const T_return_type* return_value_ptr;
    const T_return_type* getreturn()const
    {
        return this->handle.promise().return_value_ptr;
    }

};



//模板特化,无co_yield,无co_return_value,有co_return_void
template<class T_initial_await, class T_final_await,class T_yield_await>
requires    SatisfyAwait<T_initial_await>&&
SatisfyAwait<T_final_await>&&
SatisfyAwait<T_yield_await>
class BasicCoroutine<void, void, T_initial_await, T_final_await, T_yield_await>
{
public:
    struct promise_type : promise_type_base<void, void, T_initial_await, T_final_await, T_yield_await>
    {
        using Base = promise_type;

        promise_type() {}

        void return_void()
        {
            this->OnReturn();
        }

        virtual void OnReturn()
        {

        }

    };

    using HandleType = std::coroutine_handle<promise_type>;

private:
    friend class SharedCoroHandle<BasicCoroutine>;
    friend struct promise_type_base<void, void, T_initial_await, T_final_await, T_yield_await>;
    HandleType m_handle;

public:

    ~BasicCoroutine()
    {
        this->destory();
    }

    bool done()const
    {
        if (m_handle != nullptr)
            return handle.done();
        else
            return true;
    }

    void resume()const
    {
        if (m_handle != nullptr)
            handle.resume();
    }

    void destory()
    {
        if (m_handle != nullptr)
        {
            this->m_handle.destroy();
            m_handle = nullptr;
        }
    }

    bool operator==(const BasicCoroutine& right)const
    {
        return this->handle == right.handle;
    }

    bool operator==(nullptr_t)const
    {
        return m_handle == nullptr;
    }

    _declspec(property(get = gethandle)) const HandleType& handle;
    const HandleType& gethandle()const
    {
        return m_handle;
    }

};



//模板特化,无co_return,有return_void
template<class Tco_yield, class T_initial_await, class T_final_await,class T_yield_await >
requires    SatisfyAwait<T_initial_await>&&
SatisfyAwait<T_final_await>&&
SatisfyAwait<T_yield_await>
class BasicCoroutine<void, Tco_yield, T_initial_await, T_final_await, T_yield_await>
{
public:

    using T_yield_type = Tco_yield;

    struct promise_type : promise_type_base<void, Tco_yield, T_initial_await, T_final_await, T_yield_await>
    {
        using Base = promise_type;

        promise_type() {}

        ~promise_type()
        {
            delete m_yield_value;
        }

        T_yield_await yield_value(const T_yield_type& value)
        {
            this->OnYield(value);
            return {};
        }

        void return_void()
        {
            this->OnReturn();
        }

        virtual void OnYield(const T_yield_type& value)
        {
            if (m_yield_value)
            {
                *m_yield_value = value;
            }
            else
            {
                m_yield_value = new T_yield_type(value);
            }
        }

        virtual void OnReturn()
        {

        }

        _declspec(property(get = getyield)) const T_yield_type* yield_value_ptr;
        const T_yield_type* getyield()const
        {
            return this->m_yield_value;
        }

    private:
        T_yield_type* m_yield_value = nullptr;
    };

    using HandleType = std::coroutine_handle<promise_type>;

private:
    friend class SharedCoroHandle<BasicCoroutine>;
    friend struct promise_type_base<void, Tco_yield, T_initial_await, T_final_await, T_yield_await>;
    HandleType m_handle;

public:

    ~BasicCoroutine()
    {
        this->destory();
    }

    bool done()const
    {
        if (m_handle != nullptr)
            return handle.done();
        else
            return true;
    }

    void resume()const
    {
        if (m_handle != nullptr)
            handle.resume();
    }

    void destory()
    {
        if (m_handle != nullptr)
        {
            this->m_handle.destroy();
            m_handle = nullptr;
        }
    }

    bool operator==(const BasicCoroutine& right)const
    {
        return this->handle == right.handle;
    }

    bool operator==(nullptr_t)const
    {
        return m_handle == nullptr;
    }

    _declspec(property(get = gethandle)) const HandleType& handle;
    const HandleType& gethandle()const
    {
        return m_handle;
    }

    _declspec(property(get = getyield)) const T_yield_type* yield_value_ptr;
    const T_yield_type* getyield()const
    {
        return this->handle.promise().yield_value_ptr;
    }
};



//模板特化,无co_yield
template<class Tco_return, class T_initial_await, class T_final_await,class T_yield_await>
requires    SatisfyAwait<T_initial_await>&&
SatisfyAwait<T_final_await>&&
SatisfyAwait<T_yield_await>
class BasicCoroutine<Tco_return, void, T_initial_await, T_final_await, T_yield_await>
{
    friend struct BasicCoroutine::promise_type;
public:

    using T_return_type = Tco_return;

    struct promise_type : promise_type_base<Tco_return, void, T_initial_await, T_final_await, T_yield_await>
    {
        using Base = promise_type;

        promise_type() {}

        ~promise_type()
        {
            delete m_return_value;
        }

        void return_value(const T_return_type& value)
        {
            this->OnReturn(value);
        }

        virtual void OnReturn(const T_return_type& value)
        {
            m_return_value = new T_return_type(value);
        }

        _declspec(property(get = getreturn)) const T_return_type* return_value_ptr;
        const T_return_type* getreturn()const
        {
            return this->m_return_value;
        }

    private:
        T_return_type* m_return_value = nullptr;
    };

    using HandleType = std::coroutine_handle<promise_type>;

private:
    friend class SharedCoroHandle<BasicCoroutine>;
    friend struct promise_type_base<Tco_return, void, T_initial_await, T_final_await, T_yield_await>;
    HandleType m_handle;

public:

    ~BasicCoroutine()
    {
        this->destory();
    }

    bool done()const
    {
        if (m_handle != nullptr)
            return handle.done();
        else
            return true;
    }

    void resume()const
    {
        if (m_handle != nullptr)
            handle.resume();
    }

    void destory()
    {
        if (m_handle != nullptr)
        {
            this->m_handle.destroy();
            m_handle = nullptr;
        }
    }

    bool operator==(const BasicCoroutine& right)const
    {
        return this->handle == right.handle;
    }

    bool operator==(nullptr_t)const
    {
        return m_handle == nullptr;
    }

    _declspec(property(get = gethandle)) const HandleType& handle;
    const HandleType& gethandle()const
    {
        return m_handle;
    }

    _declspec(property(get = getreturn)) const T_return_type* return_value_ptr;
    const T_return_type* getreturn()const
    {
        return this->handle.promise().return_value_ptr;
    }
};

}

namespace MyCodes
{
using DefaultCoroutine = BasicCoroutine<>;

//共享协程句柄,机制类似于共享指针,负责自动管理协程的销毁
//如果协程设置为结束时自动销毁,那么使用共享句柄时,
//无法判断协程是否已经由于结束而销毁,可能导致异常
template<class CoroutineType>
class SharedCoroHandle
{
public:
    using promise_type = typename CoroutineType::promise_type;
    using HandleType = std::coroutine_handle<promise_type>;
    
    SharedCoroHandle(const SharedCoroHandle& right)
    {
        times = right.times;
        m_handle = right.m_handle;
        (*times)++;
    }

    SharedCoroHandle(const CoroutineType& coro)
    {
        times = new int(1);
        this->m_handle = coro.m_handle;
    }

    SharedCoroHandle(CoroutineType&& coro)
    {
        times = new int(1);
        this->m_handle = coro.m_handle;
        coro.m_handle = nullptr;
    }
    
    ~SharedCoroHandle()
    {
        if (times == nullptr)
            return;
    
        (*times)--;
        if (*times <= 0)
        {
            Destroy();
        }
    }
    
    const promise_type* operator->()const
    {
        return &m_handle.promise();
    }
    
    const SharedCoroHandle& operator=(const SharedCoroHandle& right)
    {
        this->~SharedCoroHandle();
        times = right.times;
        m_handle = right.m_handle;
        (*times)++;
        return *this;
    }

    const SharedCoroHandle& operator=(const CoroutineType& coro)
    {
        this->~SharedCoroHandle();
        times = new int(1);
        this->m_handle = coro.m_handle;
        return *this;
    }

    const SharedCoroHandle& operator=(CoroutineType&& coro)
    {
        this->~SharedCoroHandle();
        times = new int(1);
        this->m_handle = coro.m_handle;
        coro.m_handle = nullptr;
        return *this;
    }
    
    const bool operator==(const SharedCoroHandle& right)const
    {
        return this->m_handle == right.m_handle;
    }
    
    bool done()const
    {
        return m_handle.done();
    }
    
    void resume()const
    {
        m_handle.resume();
    }
    
    bool try_resume()const
    {
        if (!m_handle.done())
        {
            m_handle.resume();
            return true;
        }
        else
            return false;
    }
    
    _declspec(property(get = gethandle)) const HandleType& handle;
    const HandleType& gethandle()const
    {
        return this->m_handle;
    }

private:

    SharedCoroHandle() = default;

    void Destroy()
    {
        delete times;
        m_handle.destroy();
    }

    int* times=nullptr;
    HandleType m_handle;
};

    template<class CoroutineType,class... T>
    SharedCoroHandle<CoroutineType> MakeSharedCoroHandle(
    CoroutineType(*getCoroFun)(const T...),
    T... parm)
    {
        return SharedCoroHandle<CoroutineType>(getCoroFun(parm...));
    }

    template<class CoroutineType>
    SharedCoroHandle<CoroutineType> MakeSharedCoroHandle(const CoroutineType& coro)
    {
        return SharedCoroHandle<CoroutineType>(coro);
    }

    template<class CoroutineType>
    SharedCoroHandle<CoroutineType> MakeSharedCoroHandle(CoroutineType&& coro)
    {
        return SharedCoroHandle<CoroutineType>(move(coro));
    }
}

#endif
