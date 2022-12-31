/*
 * SuperTinyKernel: Minimalistic thread scheduling kernel for Embedded systems.
 *
 * Source: http://github.com/dmitrykos/stk
 *
 * Copyright (c) 2022 Dmitry Kostjucenko <dmitry.kostjucenko@gmail.com>
 * License: MIT License, see LICENSE for a full text.
 */

#include "stktest.h"

namespace stk {
namespace test {

// ============================================================================ //
// ============================= UserTask ===================================== //
// ============================================================================ //

TEST_GROUP(UserTask)
{
    void setup() {}
    void teardown() {}
};

TEST(UserTask, GetStackSize)
{
    TaskMock<ACCESS_USER> task;

    CHECK_EQUAL(TaskMock<ACCESS_USER>::STACK_SIZE, task.GetStackSize());
}

TEST_GROUP(StackMemoryWrapper)
{
    void setup() {}
    void teardown() {}
};

TEST(StackMemoryWrapper, GetStack)
{
    StackMemoryWrapper<STACK_SIZE_MIN>::MemoryType memory;
    StackMemoryWrapper<STACK_SIZE_MIN> wrapper(&memory);

    CHECK_TRUE(NULL != wrapper.GetStack());
    CHECK_EQUAL((size_t *)&memory, wrapper.GetStack());
}

TEST(StackMemoryWrapper, GetStackSize)
{
    StackMemoryWrapper<STACK_SIZE_MIN>::MemoryType memory;
    StackMemoryWrapper<STACK_SIZE_MIN> wrapper(&memory);

    CHECK_EQUAL(STACK_SIZE_MIN, wrapper.GetStackSize());
    CHECK_EQUAL(sizeof(memory) / sizeof(size_t), wrapper.GetStackSize());
}

TEST_GROUP(DList)
{
    void setup() {}
    void teardown() {}

    struct ListEntry : public stk::util::DListEntry<ListEntry>
    {
        int32_t m_id;
        ListEntry(int32_t id) : m_id(id) {}
    };

    typedef stk::util::DListHead<ListEntry> ListHead;
};

TEST(DList, Empty)
{
    ListHead list;

    CHECK_EQUAL_ZERO(list.GetSize());
    CHECK_TRUE(list.IsEmpty());
    CHECK_TRUE(NULL == list.GetFirst());
    CHECK_TRUE(NULL == list.GetLast());

    list.Clear();
}

TEST(DList, LinkFront)
{
    ListHead list;
    ListEntry e1(1), e2(2), e3(3);

    list.LinkFront(e1);
    CHECK_EQUAL(&e1, list.GetFirst());
    CHECK_EQUAL(&e1, list.GetLast());
    CHECK_EQUAL(&list, e1.GetHead());

    list.LinkFront(e2);
    CHECK_EQUAL(&e2, list.GetFirst());
    CHECK_EQUAL(&e1, list.GetLast());

    list.LinkFront(e3);
    CHECK_EQUAL(&e3, list.GetFirst());
    CHECK_EQUAL(&e1, list.GetLast());

    CHECK_EQUAL(3, list.GetSize());
}

TEST(DList, LinkBack)
{
    ListHead list;
    ListEntry e1(1), e2(2), e3(3);

    list.LinkBack(e1);
    CHECK_EQUAL(&e1, list.GetFirst());
    CHECK_EQUAL(&e1, list.GetLast());
    CHECK_EQUAL(&list, e1.GetHead());

    list.LinkBack(e2);
    CHECK_EQUAL(&e1, list.GetFirst());
    CHECK_EQUAL(&e2, list.GetLast());

    list.LinkBack(e3);
    CHECK_EQUAL(&e1, list.GetFirst());
    CHECK_EQUAL(&e3, list.GetLast());

    CHECK_EQUAL(3, list.GetSize());
}

TEST(DList, PopFront)
{
    ListHead list;
    ListEntry e1(1), e2(2), e3(3);

    list.LinkBack(e1);
    list.LinkBack(e2);
    list.LinkBack(e3);

    list.PopFront();

    CHECK_EQUAL(&e2, list.GetFirst());
    CHECK_EQUAL(&e3, list.GetLast());

    list.PopFront();

    CHECK_EQUAL(&e3, list.GetFirst());
    CHECK_EQUAL(&e3, list.GetLast());

    list.PopFront();

    CHECK_EQUAL_ZERO(list.GetSize());
    CHECK_TRUE(list.IsEmpty());
    CHECK_TRUE(NULL == list.GetFirst());
    CHECK_TRUE(NULL == list.GetLast());
}

TEST(DList, PopBack)
{
    ListHead list;
    ListEntry e1(1), e2(2), e3(3);

    list.LinkBack(e1);
    list.LinkBack(e2);
    list.LinkBack(e3);

    list.PopBack();

    CHECK_EQUAL(&e1, list.GetFirst());
    CHECK_EQUAL(&e2, list.GetLast());

    list.PopBack();

    CHECK_EQUAL(&e1, list.GetFirst());
    CHECK_EQUAL(&e1, list.GetLast());

    list.PopBack();

    CHECK_EQUAL_ZERO(list.GetSize());
    CHECK_TRUE(list.IsEmpty());
    CHECK_TRUE(NULL == list.GetFirst());
    CHECK_TRUE(NULL == list.GetLast());
}

TEST(DList, Clear)
{
    ListHead list;
    ListEntry e1(1), e2(2), e3(3);

    list.LinkBack(e1);
    list.LinkBack(e2);
    list.LinkBack(e3);

    list.Clear();

    CHECK_EQUAL_ZERO(list.GetSize());
    CHECK_TRUE(list.IsEmpty());
    CHECK_TRUE(NULL == list.GetFirst());
    CHECK_TRUE(NULL == list.GetLast());
}

TEST(DList, Iteration)
{
    ListHead list;
    ListEntry e1(1), e2(2), e3(3);

    list.LinkBack(e1);
    list.LinkBack(e2);
    list.LinkBack(e3);

    ListHead::EntryType *itr = list.GetFirst();
    CHECK_EQUAL(&e1, itr);

    itr = itr->GetNext();
    CHECK_EQUAL(&e2, itr);

    itr = itr->GetNext();
    CHECK_EQUAL(&e3, itr);

    itr = itr->GetNext();
    CHECK_EQUAL(&e1, itr);
}

TEST(DList, Relink)
{
    ListHead list;
    ListEntry e1(1), e2(2), e3(3);

    list.LinkBack(e1);
    list.LinkBack(e2);
    list.LinkBack(e3);

    ListHead list2;
    list.RelinkTo(list2);

    CHECK_EQUAL_ZERO(list.GetSize());
    CHECK_TRUE(list.IsEmpty());
    CHECK_TRUE(NULL == list.GetFirst());
    CHECK_TRUE(NULL == list.GetLast());

    CHECK_EQUAL(3, list2.GetSize());
    CHECK_EQUAL(&list2, e1.GetHead());
    CHECK_EQUAL(&list2, e2.GetHead());
    CHECK_EQUAL(&list2, e3.GetHead());
}

} // namespace stk
} // namespace test
