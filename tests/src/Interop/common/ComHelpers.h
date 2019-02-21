// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

#pragma once

#include <Windows.h>
#include <comdef.h>
#include <cassert>
#include <exception>
#include <type_traits>
#include <atomic>

// Common macro for working in COM
#define RETURN_IF_FAILED(exp) { hr = exp; if (FAILED(hr)) { return hr; } }

namespace Internal
{
    template<typename I>
    HRESULT __QueryInterfaceImpl(
        /* [in] */ REFIID riid,
        /* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject,
        /* [in] */ I obj)
    {
        if (riid == __uuidof(I))
        {
            *ppvObject = static_cast<I>(obj);
        }
        else
        {
            *ppvObject = nullptr;
            return E_NOINTERFACE;
        }

        return S_OK;
    }

    template<typename I1, typename ...IR>
    HRESULT __QueryInterfaceImpl(
        /* [in] */ REFIID riid,
        /* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject,
        /* [in] */ I1 i1,
        /* [in] */ IR... remain)
    {
        if (riid == __uuidof(I1))
        {
            *ppvObject = static_cast<I1>(i1);
            return S_OK;
        }

        return __QueryInterfaceImpl(riid, ppvObject, remain...);
    }
}

// Implementation of IUnknown operations
class UnknownImpl
{
public:
    UnknownImpl() = default;
    virtual ~UnknownImpl() = default;

    UnknownImpl(const UnknownImpl&) = delete;
    UnknownImpl& operator=(const UnknownImpl&) = delete;

    UnknownImpl(UnknownImpl&&) = default;
    UnknownImpl& operator=(UnknownImpl&&) = default;

    template<typename I1, typename ...IR>
    HRESULT DoQueryInterface(
        /* [in] */ REFIID riid,
        /* [iid_is][out] */ _COM_Outptr_ void **ppvObject,
        /* [in] */ I1 i1,
        /* [in] */ IR... remain)
    {
        if (ppvObject == nullptr)
            return E_POINTER;

        if (riid == __uuidof(IUnknown))
        {
            *ppvObject = static_cast<IUnknown *>(i1);
        }
        else
        {
            HRESULT hr = Internal::__QueryInterfaceImpl(riid, ppvObject, i1, remain...);
            if (hr != S_OK)
                return hr;
        }

        DoAddRef();
        return S_OK;
    }

    ULONG DoAddRef()
    {
        assert(_refCount > 0);
        return (++_refCount);
    }

    ULONG DoRelease()
    {
        assert(_refCount > 0);
        ULONG c = (--_refCount);
        if (c == 0)
            delete this;
        return c;
    }

private:
    std::atomic<ULONG> _refCount = 1;
};

// Marco to use for defining ref counting impls
#define DEFINE_REF_COUNTING() \
    STDMETHOD_(ULONG, AddRef)(void) { return UnknownImpl::DoAddRef(); } \
    STDMETHOD_(ULONG, Release)(void) { return UnknownImpl::DoRelease(); }

// Templated class factory
template<typename T>
class ClassFactoryBasic : public UnknownImpl, public IClassFactory
{
public: // static
    static HRESULT Create(_In_ REFIID riid, _Outptr_ LPVOID FAR* ppv)
    {
        try
        {
            auto cf = new ClassFactoryBasic();
            HRESULT hr = cf->QueryInterface(riid, ppv);
            cf->Release();
            return hr;
        }
        catch (const std::bad_alloc&)
        {
            return E_OUTOFMEMORY;
        }
    }

public: // IClassFactory
    STDMETHOD(CreateInstance)(
        _In_opt_  IUnknown *pUnkOuter,
        _In_  REFIID riid,
        _COM_Outptr_  void **ppvObject)
    {
        if (pUnkOuter != nullptr)
            return CLASS_E_NOAGGREGATION;

        try
        {
            auto ti = new T();
            HRESULT hr = ti->QueryInterface(riid, ppvObject);
            ti->Release();
            return hr;
        }
        catch (const std::bad_alloc&)
        {
            return E_OUTOFMEMORY;
        }
    }

    STDMETHOD(LockServer)(/* [in] */ BOOL fLock)
    {
        assert(false && "Not impl");
        return E_NOTIMPL;
    }

public: // IUnknown
    STDMETHOD(QueryInterface)(
        /* [in] */ REFIID riid,
        /* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject)
    {
        return DoQueryInterface(riid, ppvObject, static_cast<IClassFactory *>(this));
    }

    DEFINE_REF_COUNTING();
};

// Templated class factory for aggregation
template<typename T>
class ClassFactoryAggregate : public UnknownImpl, public IClassFactory
{
public: // static
    static HRESULT Create(_In_ REFIID riid, _Outptr_ LPVOID FAR* ppv)
    {
        try
        {
            auto cf = new ClassFactoryAggregate();
            HRESULT hr = cf->QueryInterface(riid, ppv);
            cf->Release();
            return hr;
        }
        catch (const std::bad_alloc&)
        {
            return E_OUTOFMEMORY;
        }
    }

public: // IClassFactory
    STDMETHOD(CreateInstance)(
        _In_opt_  IUnknown *pUnkOuter,
        _In_  REFIID riid,
        _COM_Outptr_  void **ppvObject)
    {
        if (pUnkOuter != nullptr && riid != IID_IUnknown)
            return CLASS_E_NOAGGREGATION;

        try
        {
            auto ti = new T(pUnkOuter);
            HRESULT hr = ti->QueryInterface(riid, ppvObject);
            ti->Release();
            return hr;
        }
        catch (const std::bad_alloc&)
        {
            return E_OUTOFMEMORY;
        }
    }

    STDMETHOD(LockServer)(/* [in] */ BOOL fLock)
    {
        assert(false && "Not impl");
        return E_NOTIMPL;
    }

public: // IUnknown
    STDMETHOD(QueryInterface)(
        /* [in] */ REFIID riid,
        /* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject)
    {
        return DoQueryInterface(riid, ppvObject, static_cast<IClassFactory *>(this));
    }

    DEFINE_REF_COUNTING();
};