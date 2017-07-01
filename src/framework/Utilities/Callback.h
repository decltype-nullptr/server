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

#ifndef MANGOS_CALLBACK_H
#define MANGOS_CALLBACK_H

#include <memory>

// defines to simplify multi param templates code and readablity
#define TYPENAMES_1 typename T1
#define TYPENAMES_2 TYPENAMES_1, typename T2
#define TYPENAMES_3 TYPENAMES_2, typename T3
#define TYPENAMES_4 TYPENAMES_3, typename T4
#define TYPENAMES_5 TYPENAMES_4, typename T5
#define TYPENAMES_6 TYPENAMES_5, typename T6
#define TYPENAMES_7 TYPENAMES_6, typename T7
#define TYPENAMES_8 TYPENAMES_7, typename T8
#define TYPENAMES_9 TYPENAMES_8, typename T9
#define TYPENAMES_10 TYPENAMES_9, typename T10

#define PARAMS_1 T1 param1
#define PARAMS_2 PARAMS_1, T2 param2
#define PARAMS_3 PARAMS_2, T3 param3
#define PARAMS_4 PARAMS_3, T4 param4
#define PARAMS_5 PARAMS_4, T5 param5
#define PARAMS_6 PARAMS_5, T6 param6
#define PARAMS_7 PARAMS_6, T7 param7
#define PARAMS_8 PARAMS_7, T8 param8
#define PARAMS_9 PARAMS_8, T9 param9
#define PARAMS_10 PARAMS_9, T10 param10

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
        virtual void SetResult(std::shared_ptr<QueryResult> result) = 0;
        virtual std::shared_ptr<QueryResult> GetResult() const      = 0;
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

        void SetResult(std::shared_ptr<QueryResult> result)
        {
            const_cast<std::shared_ptr<QueryResult>&>(std::get<0>(CB::m_params)) = result;
        }

        std::shared_ptr<QueryResult> GetResult() const { return std::get<0>(CB::m_params); }
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

#endif
