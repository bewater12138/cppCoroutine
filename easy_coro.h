/*
    ����c++20�����ԣ�Э����д�Ŀ⣬��������ģ��ķ�ʽ��װ��c++ԭ��Э�̵ľ���
ʵ�֣��Թ�����ʹ�á�

    Э����ģ���������ģ�����:
        Tco_return
        Tco_yield
        T_initial_await
        T_final_await
        T_yield_await

        ���У�ǰ����ģ������ֱ�ΪЭ�̵� return ���ͺ� yield ���͡�
        ��������ģ�������Ӧ����һ��Լ�����ڱ������� SatisfyAwait Լ������
    ������ģ����������ж���ָ���������Ƿ�Ҫ����Э�̡�
        
    Э����ģ�������ǰ����ģ�����������Ӧƫ�ػ�������� void ���ͣ���
    ��� return �� yield ��ֵʱ����ָ����ʽ�洢��

    ��Ҫ��չЭ���࣬��Ӧ���Զ��������У�����һ����Ϊ promise_type �����ͣ�
�� promise_type ����Ӧ���̳��� BasicCoroutine::promise_type<> ���͡���
����������ѡ��һ��Ϊ�Զ������������ BasicCoroutine<> ����ת������ʽת��
�������Զ���������Ӧ������Ӧ�ĳ�Ա����� BasicCoroutine<> �е�Э�̾������
�������� get_return_object ���������Ǻ�ķ���ֵӦΪ�Զ������͡�׼���������
�󣬼��ɿ�ʼ��д��Ӧ���鷽���ˣ������鷽����ǩ��������������˺궨�壬���Է�
�����д����Щ�ṩ���鷽��������Ӧʱ�����ض��ķ������ã���ʵ����������

    ����Э�̾����Ҫ�ֶ��ͷţ���˱����и��ݹ���ָ��Ļ����ṩ�˹���Э�̾����
�Ա�����Զ�����Э�̵����١�����ʱͨ�� MakeSharedCoroHandle ��������,�ú���
�Ĳ���Ϊ����Э�̵ĺ���ָ�룬��Ӧ�޲�������Ҫʹ���в�����Э�̴�������������ʹ
���޲���� lambda ����һ�η�װ������û��Ϊ����Э�̾���ṩƫ�ػ�������޷�ֱ��
�õ� return �� yield ��ֵ������Ӧ�������ص� -> ������õ���

*/
#pragma once
#if _MSVC_LANG > 201703L
#include<coroutine>

#define _AWAIT std::suspend_always()
#define _CONTINUE std::suspend_never()

#define CO_AWAIT co_await _AWAIT
#define CO_CONTINUE co_await _CONTINUE

#define OVERRIDE_ON_RETURN_VOID virtual void OnReturn()override
#define OVERRIDE_ON_RETURN virtual void OnReturn(const T_return_type& value)override
#define OVERRIDE_ON_YIELD virtual void OnYield(const T_yield_type& value)override
#define OVERRIDE_ON_GET_OBJ_FUN virtual void OnGetObject()override
#define OVERRIDE_ON_INITIAL_FUN virtual void OnInitial()override
#define OVERRIDE_ON_FINAL_FUN virtual void OnFinal()noexcept override


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
class BasicCoroutine;

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
    CoroType resault;
    resault.handle = std::coroutine_handle<typename CoroType::promise_type>::
        from_promise(*
            (reinterpret_cast<typename CoroType::promise_type*>(this))
        );

    return resault;
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
}

namespace MyCodes
{
//Э��ģ��,Tco_return��co_return�ķ���ֵ����,
//Tco_yield��co_yield�ķ�������
//T_pt������ģ�����������await_ready���������ͣ������жϸ���������Ƿ����Э��
//Ĭ��Ϊstd::suspend_always,����������Щʱ�����
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

std::coroutine_handle<promise_type> handle;

~BasicCoroutine()
{

}

bool done()const
{
    return handle.done();
}

void resume()const
{
    handle.resume();
}

void destory()const
{
    this->handle.destroy();
}

bool operator==(const BasicCoroutine& right)const
{
    return this->handle == right.handle;
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



//ģ���ػ�,��co_yield,��co_return_value,��co_return_void
template<class T_initial_await, class T_final_await,class T_yield_await>
requires    SatisfyAwait<T_initial_await>&&
            SatisfyAwait<T_final_await>&&
            SatisfyAwait<T_yield_await>
class BasicCoroutine<void, void, T_initial_await, T_final_await,T_yield_await>
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

virtual ~BasicCoroutine()
{

}

bool done()const
{
    return handle.done();
}

void resume()const
{
    handle.resume();
}

void destory()const
{
    this->handle.destroy();
}
bool operator==(const BasicCoroutine& right)const
{
    return this->handle == right.handle;
}

std::coroutine_handle<promise_type> handle;

};



//ģ���ػ�,��co_return,��return_void
template<class Tco_yield, class T_initial_await, class T_final_await,class T_yield_await >
requires    SatisfyAwait<T_initial_await>&&
            SatisfyAwait<T_final_await>&&
            SatisfyAwait<T_yield_await>
class BasicCoroutine<void, Tco_yield, T_initial_await, T_final_await,T_yield_await>
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

virtual ~BasicCoroutine()
{

}

bool done()const
{
    return handle.done();
}

void resume()const
{
    handle.resume();
}

void destory()const
{
    this->handle.destroy();
}

bool operator==(const BasicCoroutine& right)const
{
    return this->handle == right.handle;
}

std::coroutine_handle<promise_type> handle;

_declspec(property(get = getyield)) const T_yield_type* yield_value_ptr;
const T_yield_type* getyield()const
{
    return this->handle.promise().yield_value_ptr;
}
};



//ģ���ػ�,��co_yield
template<class Tco_return, class T_initial_await, class T_final_await,class T_yield_await>
requires    SatisfyAwait<T_initial_await>&&
            SatisfyAwait<T_final_await>&&
            SatisfyAwait<T_yield_await>
class BasicCoroutine<Tco_return, void, T_initial_await, T_final_await,T_yield_await>
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

virtual ~BasicCoroutine()
{

}

bool done()const
{
    return handle.done();
}

void resume()const
{
    handle.resume();
}

void destory()const
{
    this->handle.destroy();
}

bool operator==(const BasicCoroutine& right)const
{
    return this->handle == right.handle;
}

std::coroutine_handle<promise_type> handle;

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

template<class CoroutineType>
class SharedCoHandle;

template<class CoroutineType>
SharedCoHandle<CoroutineType> MakeSharedCoroHandle(CoroutineType(*getCorFun)());

//����Э�̾��,���������ڹ���ָ��,�����Զ�����Э�̵�����
//���Э������Ϊ����ʱ�Զ�����,��ôʹ�ù�����ʱ,
//�޷��ж�Э���Ƿ��Ѿ����ڽ���������,���ܵ����쳣
template<class CoroutineType>
class SharedCoHandle
{
public:
using promise_type = typename CoroutineType::promise_type;
using HandleType = std::coroutine_handle<promise_type>;

SharedCoHandle() = delete;  //��ֹĬ�Ϲ���

SharedCoHandle(const SharedCoHandle& right)
{
    times = right.times;
    m_handle = right.m_handle;
    (*times)++;
}

SharedCoHandle(const SharedCoHandle&& right)
{
    times = right.times;
    m_handle = right.m_handle;
    (*times)++;
}

~SharedCoHandle()
{
    if (times == nullptr)
        return;

    (*times)--;
    if (*times <= 0)
    {
        delete times;
        m_handle.destroy();
    }
}

const promise_type* operator->()const
{
    return &m_handle.promise();
}

const SharedCoHandle& operator=(const SharedCoHandle& right)
{
    this->~SharedCoHandle();
    times = right.times;
    m_handle = right.m_handle;
    (*times)++;
    return *this;
}

const SharedCoHandle& operator=(const SharedCoHandle&& right)
{
    this->~SharedCoHandle();
    times = right.times;
    m_handle = right.m_handle;
    (*times)++;
    return *this;
}

const bool operator==(const SharedCoHandle& right)const
{
    return this->m_handle == right.m_handle;
}

const bool operator==(const SharedCoHandle&& right)const
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
friend SharedCoHandle<CoroutineType> MakeSharedCoroHandle<CoroutineType>(CoroutineType(*getCorFun)());
SharedCoHandle(const HandleType handle)
{
    times = new int(1);
    this->m_handle = handle;
}

int* times=nullptr;
HandleType m_handle;
};


template<class CoroutineType>
SharedCoHandle<CoroutineType> MakeSharedCoroHandle(CoroutineType(*getCorFun)())
{
auto thecor = getCorFun();
return SharedCoHandle<CoroutineType>(thecor.handle);
}
}

#endif