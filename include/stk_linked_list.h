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
    DListEntry() : m_head(NULL), m_next(NULL), m_prev(NULL)
    { }
    virtual ~DListEntry()
    { }

    typedef DListEntry<_Ty> EntryType;
    typedef DListHead<_Ty>  HeadType;

    HeadType *GetHead() const  { return m_head; }
    EntryType *GetNext() const { return m_next; }
    EntryType *GetPrev() const { return m_prev; }
    bool IsLinked() const      { return (GetHead() != NULL); }

    operator _Ty *()             { return (_Ty *)this; }
    operator const _Ty *() const { return (_Ty *)this; }

private:
    void Link(HeadType *head, EntryType *next, EntryType *prev)
    {
        m_head = head;
        m_next = next;
        m_prev = prev;

        if (m_prev != NULL)
            m_prev->m_next = this;

        if (m_next != NULL)
            m_next->m_prev = this;
    }

    void Unlink()
    {
        if (m_prev != NULL)
            m_prev->m_next = m_next;

        if (m_next != NULL)
            m_next->m_prev = m_prev;

        m_head = NULL;
        m_next = NULL;
        m_prev = NULL;
    }

    HeadType  *m_head; //!< list head
    EntryType *m_next; //!< next list entry
    EntryType *m_prev; //!< previous list entry
};

//! Double linked intrusive list head.
template <class _Ty> class DListHead
{
    friend class DListEntry<_Ty>;

public:
    typedef DListEntry<_Ty> EntryType;

    explicit DListHead(): m_count(0), m_first(NULL), m_last(NULL)
    { }

    size_t GetSize() const           { return m_count; }
    bool IsEmpty() const             { return (m_count == 0); }

    EntryType *GetFirst() const      { return m_first; }
    EntryType *GetLast() const       { return m_last; }

    void Clear()                     { while (!IsEmpty()) Unlink(GetFirst()); }

    void LinkBack(EntryType *entry)  { Link(entry, NULL, GetLast()); }
    void LinkFront(EntryType *entry) { Link(entry, GetLast(), NULL); }

    EntryType *PopBack()             { EntryType *ret = GetLast(); Unlink(ret); return ret; }
    EntryType *PopFront()            { EntryType *ret = GetFirst(); Unlink(ret); return ret; }

    void Unlink(EntryType *entry)
    {
        assert(entry->IsLinked());
        assert(entry->GetHead() == this);

        if (m_first == entry)
            m_first = entry->GetNext();

        if (m_last == entry)
            m_last = entry->GetPrev();

        UpdateLoop();

        entry->Remove();
        --m_count;
    }

    void RelinkTo(DListHead &to)
    {
        assert(&to != this);

        while (!this->IsEmpty())
        {
            to.PushBack(this->PopFront());
        }
    }

private:
    void Link(EntryType *entry, EntryType *next = NULL, EntryType *prev = NULL)
    {
        assert(!entry->IsLinked());

        if (prev == NULL)
            next = m_first;

        ++m_count;
        entry->Link(this, next, prev);

        if ((m_first == NULL) || (m_first == entry->GetNext()))
            m_first = entry;

        if ((m_last == NULL) || (m_last == entry->GetPrev()))
            m_last = entry;

        UpdateLoop();
    }

    void UpdateLoop()
    {
        if (IsEmpty())
        {
            m_first = NULL;
            m_last = NULL;
        }
        else
        {
            m_first->m_prev = m_last;
            m_last->m_next = m_first;
        }
    }

    size_t     m_count; //!< number of linked elements
    EntryType *m_first; //!< first element
    EntryType *m_last;  //!< last element
};

} // namespace util
} // namespace stk


#endif /* STK_LINKED_LIST_H_ */
