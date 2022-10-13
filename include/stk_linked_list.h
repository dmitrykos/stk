/*
 * SuperTinyKernel: Minimalistic thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022 Dmitry Kostjucenko <dmitry.kostjucenko@gmail.com>
 * License: MIT License, see LICENSE for a full text.
 */

#ifndef STK_LINKED_LIST_H_
#define STK_LINKED_LIST_H_

#include <assert.h>

namespace stk {
namespace util {

template <class _Ty> class DListHead;

//! Double linked intrusive list entry.
template <class _Ty> class DListEntry
{
    friend class DListHead<_Ty>;

public:
    typedef DListEntry<_Ty> EntryT;
    typedef DListHead<_Ty>  HeadT;

private:
    HeadT  *m_head;
    EntryT *m_prev;
    EntryT *m_next;

public:
    DListEntry() : m_head(NULL), m_prev(NULL), m_next(NULL)
    { }
    virtual ~DListEntry()
    { }

    EntryT *GetPrev() const { return m_prev; }
    EntryT *GetNext() const { return m_next; }
    HeadT *GetHead() const  { return m_head; }
    bool IsAdded() const    { return (GetHead() != NULL); }

    operator _Ty *()             { return (_Ty *)this; }
    operator const _Ty *() const { return (_Ty *)this; }

private:
    void Add(EntryT *prev, EntryT *next, HeadT *head)
    {
        m_prev = prev;
        m_next = next;

        if (m_prev)
            m_prev->m_next = this;

        if (m_next)
            m_next->m_prev = this;

        m_head = head;
    }

    void Remove()
    {
        if (m_prev)
            m_prev->m_next = m_next;

        if (m_next)
            m_next->m_prev = m_prev;

        m_prev = NULL;
        m_next = NULL;

        m_head = NULL;
    }
};

//! Double linked intrusive list head.
template <class _Ty> class DListHead
{
    friend class DListEntry<_Ty>;

public:
    typedef DListEntry<_Ty> EntryT;

    explicit DListHead(): m_count(0), m_first(NULL), m_last(NULL)
    { }

    uint32_t GetSize() const      { return m_count; }
    bool IsEmpty() const          { return (m_count == 0); }

    EntryT *GetFirst() const      { return m_first; }
    EntryT *GetLast() const       { return m_last; }

    void Clear()                  { while (!IsEmpty()) Remove(GetFirst()); }

    void PushBack(EntryT *entry)  { Add(entry, GetLast(), NULL); }
    void PushFront(EntryT *entry) { Add(entry, NULL, GetLast()); }

    EntryT *PopBack()             { EntryT *ret = GetLast(); Remove(ret); return ret; }
    EntryT *PopFront()            { EntryT *ret = GetFirst(); Remove(ret); return ret; }

    void Remove(EntryT *entry)
    {
        assert(entry->IsAdded());
        assert(entry->GetHead() == this);

        if (m_first == entry)
            m_first = entry->GetNext();

        if (m_last == entry)
            m_last = entry->GetPrev();

        entry->Remove();
        --m_count;
    }

    void RelinkTo(DListHead &to)
    {
        while (!this->IsEmpty())
        {
            to.PushBack(this->PopFront());
        }
    }

private:
    void Add(EntryT *entry, EntryT *prev = NULL, EntryT *next = NULL)
    {
        assert(!entry->IsAdded());

        if (prev == NULL)
            next = m_first;

        ++m_count;
        entry->Add(prev, next, this);

        if ((m_first == NULL) || (m_first == entry->GetNext()))
            m_first = entry;

        if ((m_last == NULL) || (m_last == entry->GetPrev()))
            m_last = entry;
    }

    uint32_t m_count; //!< number of linked elements
    EntryT  *m_first; //!< first element
    EntryT  *m_last;  //!< last element
};

} // namespace util
} // namespace stk


#endif /* STK_LINKED_LIST_H_ */
