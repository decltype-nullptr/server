/*
 * Copyright (C) 2005-2011 MaNGOS <http://getmangos.com/>
 * Copyright (C) 2009-2011 MaNGOSZero <https://github.com/mangos/zero>
 * Copyright (C) 2011-2016 Nostalrius <https://nostalrius.org>
 * Copyright (C) 2016-2017 Elysium Project <https://github.com/elysium-project>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#pragma once

namespace std
{
    template <typename T>
    class shared_ptr;
}

// empty struct to use in templates instead of void type
struct null
{
    null() {}
};
/// ------------ BASE CLASSES ------------

#define HAS_VARIADIC_TEMPLATES 1

namespace MaNGOS
{
    template <typename Class, typename... Args>
    class _Callback
    {
    protected:
        using Method = void (Class::*)(Args...);

        Class* m_object;
        Method m_method;
        std::tuple<Args...> m_params;

        void _Execute() const { _Unwrap(std::index_sequence_for<Args...>{}); }

        template <size_t... Index>
        void _Unwrap(std::index_sequence<Index...>) const
        {
            (m_object->*m_method)(std::get<Index>(m_params)...);
        }

    public:
        explicit _Callback(Class* object, Method method, Args... params)
            : m_object(object)
            , m_method(method)
            , m_params(params...)
        {
        }

        _Callback(_Callback&& other)
            : m_object(std::move(other.m_object))
            , m_method(std::move(other.m_method))
            , m_params(std::move(other.m_params))
        {
        }
    };

    template <typename... Args>
    class _SCallback
    {
    protected:
        using Method = void (*)(Args...);

        Method m_method;
        std::tuple<Args...> m_params;

        void _Execute() const { _Unwrap(std::index_sequence_for<Args...>{}); }

        template <size_t... Index>
        void _Unwrap(std::index_sequence<Index...>) const
        {
            (*m_method)(std::get<Index>(m_params)...);
        }

    public:
        explicit _SCallback(Method method, Args... params)
            : m_method(method)
            , m_params(params...)
        {
        }

        _SCallback(_SCallback&& other)
            : m_method(std::move(other.m_method))
            , m_params(std::move(other.m_params))
        {
        }
    };
}

/// --------- GENERIC CALLBACKS ----------

namespace MaNGOS
{
    class ICallback
    {
    public:
        virtual void Execute() = 0;
        virtual ~ICallback() {}
    };

    template <class CB>
    class _ICallback : public CB, public ICallback
    {
    public:
        _ICallback(CB&& cb)
            : CB(std::move(cb))
        {
        }

        void Execute() { CB::_Execute(); }
    };

    template <typename Class, typename... Args>
    class Callback : public _ICallback<_Callback<Class, Args...>>
    {
    private:
        using TCallback = _Callback<Class, Args...>;
        using Super     = _ICallback<TCallback>;
        using Method    = typename TCallback::Method;

    public:
        Callback(Class* object, Method method, Args... params)
            : Super(TCallback(object, method, params...))
        {
        }
    };
}

/// ---------- QUERY CALLBACKS -----------

class QueryResult;

namespace MaNGOS
{
    class IQueryCallback
    {
    public:
        IQueryCallback()
            : threadSafe(true)
        {
        }
        virtual void Execute() = 0;
        virtual ~IQueryCallback() {}
        virtual void SetResult(const std::shared_ptr<QueryResult>& result) = 0;
        virtual const std::shared_ptr<QueryResult>& GetResult() const      = 0;
        bool IsThreadSafe() const { return threadSafe; }
        bool threadSafe;
    };

    template <class CB>
    class _IQueryCallback : public CB, public IQueryCallback
    {
    public:
        _IQueryCallback(CB&& cb)
            : CB(std::move(cb))
        {
        }

        void Execute() { CB::_Execute(); }

        void SetResult(const std::shared_ptr<QueryResult>& result)
        {
            const_cast<std::shared_ptr<QueryResult>&>(std::get<0>(CB::m_params)) = result;
        }

        const std::shared_ptr<QueryResult>& GetResult() const { return std::get<0>(CB::m_params); }
    };

    template <typename Class, typename... Args>
    class QueryCallback
        : public _IQueryCallback<_Callback<Class, const std::shared_ptr<QueryResult>&, Args...>>
    {
    private:
        using TCallback = _Callback<Class, const std::shared_ptr<QueryResult>&, Args...>;
        using Super     = _IQueryCallback<TCallback>;
        using Method    = typename TCallback::Method;

    public:
        QueryCallback(Class* object,
                      Method method,
                      const std::shared_ptr<QueryResult>& result,
                      Args... params)
            : Super(TCallback(object, method, result, params...))
        {
        }
    };

    template <typename... Args>
    class SQueryCallback
        : public _IQueryCallback<_SCallback<const std::shared_ptr<QueryResult>&, Args...>>
    {
    private:
        using TCallback = _SCallback<const std::shared_ptr<QueryResult>&, Args...>;
        using Super     = _IQueryCallback<TCallback>;
        using Method    = typename TCallback::Method;

    public:
        SQueryCallback(Method method, const std::shared_ptr<QueryResult>& result, Args... params)
            : Super(TCallback(method, result, params...))
        {
        }
    };
}