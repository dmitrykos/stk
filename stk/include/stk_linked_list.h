/*
 * SuperTinyKernel: Minimalistic C++ thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2025 Neutron Code Limited <stk@neutroncode.com>
 * License: MIT License, see LICENSE for a full text.
 */

#ifndef STK_LINKED_LIST_H_
#define STK_LINKED_LIST_H_

/*! \file  stk_linked_list.h
    \brief Contains linked list implementation.
*/

namespace stk {
namespace util {

template <class _Ty, bool _ClosedLoop> class DListHead;

/*! \class DListEntry
    \brief Double linked intrusive list entry.
*/
template <class _Ty, bool _ClosedLoop> class DListEntry
{
    friend class DListHead<_Ty, _ClosedLoop>;

public:
    explicit DListEntry() : m_head(NULL), m_next(NULL), m_prev(NULL)
    {}

    typedef DListEntry<_Ty, _ClosedLoop> DLEntryType;
    typedef DListHead<_Ty, _ClosedLoop>  DLHeadType;

    DLHeadType *GetHead() const  { return m_head; }
    DLEntryType *GetNext() const { return m_next; }
    DLEntryType *GetPrev() const { return m_prev; }
    bool IsLinked() const        { return (GetHead() != NULL); }

    operator _Ty *()             { return static_cast<_Ty *>(this); }
    operator const _Ty *() const { return static_cast<_Ty *>(this); }

protected:
    /*! \brief     Default destructor.
        \note      Entry can not be deleted directly, only through the host entry.
                   It is non-virtual to avoid bloating binary with stdc++ dependency
                   therefore it can not be used for a deletion of the parent object.
    */
    ~DListEntry()
    {}

private:
    void Link(DLHeadType *head, DLEntryType *next, DLEntryType *prev)
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

    DLHeadType  *m_head; //!< list head
    DLEntryType *m_next; //!< next list entry
    DLEntryType *m_prev; //!< previous list entry
};

/*! \class DListHead
    \brief Double linked intrusive list head.
*/
template <class _Ty, bool _ClosedLoop> class DListHead
{
    friend class DListEntry<_Ty, _ClosedLoop>;

public:
    typedef DListEntry<_Ty, _ClosedLoop> DLEntryType;

    explicit DListHead(): m_count(0), m_first(NULL), m_last(NULL)
    {}

    size_t GetSize() const             { return m_count; }
    bool IsEmpty() const               { return (m_count == 0); }

    DLEntryType *GetFirst() const      { return m_first; }
    DLEntryType *GetLast() const       { return m_last; }

    void Clear()                       { while (!IsEmpty()) Unlink(m_first); }

    void LinkBack(DLEntryType *entry)  { Link(entry, NULL, m_last); }
    void LinkBack(DLEntryType &entry)  { Link(&entry, NULL, m_last); }

    void LinkFront(DLEntryType *entry) { Link(entry, m_last, NULL); }
    void LinkFront(DLEntryType &entry) { Link(&entry, m_last, NULL); }

    DLEntryType *PopBack()
    {
        DLEntryType *ret = m_last;
        Unlink(ret);
        return ret;
    }

    DLEntryType *PopFront()
    {
        DLEntryType *ret = m_first;
        Unlink(ret);
        return ret;
    }

    void Unlink(DLEntryType *entry)
    {
        STK_ASSERT(entry != NULL);
        STK_ASSERT(entry->IsLinked());
        STK_ASSERT(entry->GetHead() == this);

        if (m_first == entry)
            m_first = entry->GetNext();

        if (m_last == entry)
            m_last = entry->GetPrev();

        entry->Unlink();
        --m_count;

        UpdateEnds();
    }

    void RelinkTo(DListHead &to)
    {
        STK_ASSERT(&to != this);

        while (!this->IsEmpty())
        {
            to.LinkBack(this->PopFront());
        }
    }

    void Link(DLEntryType *entry, DLEntryType *next = NULL, DLEntryType *prev = NULL)
    {
        STK_ASSERT(entry != NULL);
        STK_ASSERT(!entry->IsLinked());

        if (prev == NULL)
            next = m_first;

        ++m_count;
        entry->Link(this, next, prev);

        if ((m_first == NULL) || (m_first == entry->GetNext()))
            m_first = entry;

        if ((m_last == NULL) || (m_last == entry->GetPrev()))
            m_last = entry;

        if (_ClosedLoop)
            UpdateEnds();
    }

private:
    void UpdateEnds()
    {
        if (IsEmpty())
        {
            m_first = NULL;
            m_last = NULL;
        }
        else
        if (_ClosedLoop)
        {
            m_first->m_prev = m_last;
            m_last->m_next = m_first;
        }
    }

    size_t       m_count; //!< number of linked elements
    DLEntryType *m_first; //!< first element
    DLEntryType *m_last;  //!< last element
};

} // namespace util
} // namespace stk


#endif /* STK_LINKED_LIST_H_ */
