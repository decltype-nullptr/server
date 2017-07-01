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

#include "Database/Database.h"
#include "Database/SqlOperations.h"

/// Function body definitions for the template function members of the Database class

#define ASYNC_QUERY_BODY(sql)    \
    if (!sql || !m_pResultQueue) \
        return false;
#define ASYNC_DELAYHOLDER_BODY(holder) \
    if (!holder || !m_pResultQueue)    \
        return false;

// -- Query / member --

template <typename Class, typename... Args>
inline bool Database::AsyncQuery(const char* sql,
                                 Class* object,
                                 void (Class::*method)(const std::shared_ptr<QueryResult>&,
                                                       Args...),
                                 Args... params)
{
    ASYNC_QUERY_BODY(sql)
    AddToDelayQueue(
        new SqlQuery(sql,
                     new MaNGOS::QueryCallback<Class, Args...>(object, method, nullptr, params...),
                     m_pResultQueue));
    return true;
}

template <typename... Args>
inline bool Database::AsyncQuery(const char* sql,
                                 void (*method)(const std::shared_ptr<QueryResult>&, Args...),
                                 Args... params)
{
    ASYNC_QUERY_BODY(sql)
    AddToDelayQueue(new SqlQuery(
        sql, new MaNGOS::SQueryCallback<Args...>(method, nullptr, params...), m_pResultQueue));
    return true;
}

template <typename Class, typename... Args>
inline bool Database::AsyncQueryUnsafe(const char* sql,
                                       Class* object,
                                       void (Class::*method)(const std::shared_ptr<QueryResult>&,
                                                             Args...),
                                       Args... params)
{
    ASYNC_QUERY_BODY(sql)
    auto cb        = new MaNGOS::QueryCallback<Class, Args...>(object, method, nullptr, params...);
    cb->threadSafe = false;
    AddToDelayQueue(new SqlQuery(sql, cb, m_pResultQueue));
    return true;
}

template <typename... Args>
inline bool Database::AsyncQueryUnsafe(const char* sql,
                                       void (*method)(const std::shared_ptr<QueryResult>&, Args...),
                                       Args... params)
{
    ASYNC_QUERY_BODY(sql)
    auto cb        = new MaNGOS::SQueryCallback<Args...>(method, nullptr, params...);
    cb->threadSafe = false;
    AddToDelayQueue(new SqlQuery(sql, cb, m_pResultQueue));
    return true;
}

// -- PQuery / member --

template <typename Class, typename... Args>
inline bool Database::AsyncPQuery(QueryFormatter&& formatter,
                                  Class* object,
                                  void (Class::*method)(const std::shared_ptr<QueryResult>&,
                                                        Args...),
                                  Args... params)
{
    return AsyncQuery(formatter.m_buffer, object, method, params...);
}

template <typename Class, typename... Args>
inline bool Database::AsyncPQueryUnsafe(QueryFormatter&& formatter,
                                        Class* object,
                                        void (Class::*method)(const std::shared_ptr<QueryResult>&,
                                                              Args...),
                                        Args... params)
{
    return AsyncQueryUnsafe(formatter.m_buffer, object, method, params...);
}

// -- PQuery / static --

template <typename... Args>
inline bool Database::AsyncPQuery(QueryFormatter&& formatter,
                                  void (*method)(const std::shared_ptr<QueryResult>&, Args...),
                                  Args... params)
{
    return AsyncQuery(formatter.m_buffer, method, params...);
}

template <typename... Args>
inline bool Database::AsyncPQueryUnsafe(QueryFormatter&& formatter,
                                        void (*method)(const std::shared_ptr<QueryResult>&,
                                                       Args...),
                                        Args... params)
{
    return AsyncQueryUnsafe(formatter.m_buffer, method, params...);
}

template <typename... Args>
inline bool Database::PExecute(const char* format, Args... params)
{
    if (!format)
        return false;

    char szQuery[ MAX_QUERY_LEN ];
    int res = snprintf(szQuery, MAX_QUERY_LEN, format, params...);

    if (res == -1)
    {
        sLog.outError("SQL Query truncated (and not execute) for format: %s", format);
        return false;
    }

    return Execute(szQuery);
}

template <typename... Args>
inline bool Database::PExecuteLog(const char* format, Args... params)
{
    if (!format)
        return false;

    char szQuery[ MAX_QUERY_LEN ];
    int res = snprintf(szQuery, MAX_QUERY_LEN, format, params...);

    if (res == -1)
    {
        sLog.outError("SQL Query truncated (and not execute) for format: %s", format);
        return false;
    }

    if (m_logSQL)
    {
        time_t curr;
        tm local;
        time(&curr);                 // get current time_t value
        local = *(localtime(&curr)); // dereference and assign
        char fName[ 128 ];
        sprintf(fName,
                "%04d-%02d-%02d_logSQL.sql",
                local.tm_year + 1900,
                local.tm_mon + 1,
                local.tm_mday);

        FILE* log_file;
        std::string logsDir_fname = m_logsDir + fName;
        log_file                  = fopen(logsDir_fname.c_str(), "a");
        if (log_file)
        {
            fprintf(log_file, "%s;\n", szQuery);
            fclose(log_file);
        }
        else
        {
            // The file could not be opened
            sLog.outError(
                "SQL-Logging is disabled - Log file for the SQL commands could not be openend: %s",
                fName);
        }
    }

    return Execute(szQuery);
}

// -- QueryHolder --

template <class Class>
bool Database::DelayQueryHolder(Class* object,
                                void (Class::*method)(const std::shared_ptr<QueryResult>&,
                                                      SqlQueryHolder*),
                                SqlQueryHolder* holder)
{
    ASYNC_DELAYHOLDER_BODY(holder)
    return holder->Execute(
        new MaNGOS::QueryCallback<Class, SqlQueryHolder*>(object, method, nullptr, holder),
        this,
        m_pResultQueue);
}
template <class Class>
bool Database::DelayQueryHolderUnsafe(Class* object,
                                      void (Class::*method)(const std::shared_ptr<QueryResult>&,
                                                            SqlQueryHolder*),
                                      SqlQueryHolder* holder)
{
    ASYNC_DELAYHOLDER_BODY(holder)
    MaNGOS::QueryCallback<Class, SqlQueryHolder*>* cb =
        new MaNGOS::QueryCallback<Class, SqlQueryHolder*>(object, method, nullptr, holder);
    cb->threadSafe = false;
    return holder->Execute(cb, this, m_pResultQueue);
}
template <class Class, typename ParamType1>
bool Database::DelayQueryHolder(Class* object,
                                void (Class::*method)(const std::shared_ptr<QueryResult>&,
                                                      SqlQueryHolder*,
                                                      ParamType1),
                                SqlQueryHolder* holder,
                                ParamType1 param1)
{
    ASYNC_DELAYHOLDER_BODY(holder)
    return holder->Execute(new MaNGOS::QueryCallback<Class, SqlQueryHolder*, ParamType1>(
                               object, method, nullptr, holder, param1),
                           this,
                           m_pResultQueue);
}

#undef ASYNC_QUERY_BODY
#undef ASYNC_PQUERY_BODY
#undef ASYNC_DELAYHOLDER_BODY
