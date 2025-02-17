/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PropertyTable.h"

#include "JSCJSValueInlines.h"

namespace JSC {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(PropertyTable);

const ClassInfo PropertyTable::s_info = { "PropertyTable"_s, nullptr, nullptr, nullptr, CREATE_METHOD_TABLE(PropertyTable) };

PropertyTable* PropertyTable::create(VM& vm, unsigned initialCapacity)
{
    PropertyTable* table = new (NotNull, allocateCell<PropertyTable>(vm)) PropertyTable(vm, initialCapacity);
    table->finishCreation(vm);
    return table;
}

PropertyTable* PropertyTable::clone(VM& vm, const PropertyTable& other)
{
    PropertyTable* table = new (NotNull, allocateCell<PropertyTable>(vm)) PropertyTable(vm, other);
    table->finishCreation(vm);
    return table;
}

PropertyTable* PropertyTable::clone(VM& vm, unsigned initialCapacity, const PropertyTable& other)
{
    PropertyTable* table = new (NotNull, allocateCell<PropertyTable>(vm)) PropertyTable(vm, initialCapacity, other);
    table->finishCreation(vm);
    return table;
}

PropertyTable::PropertyTable(VM& vm, unsigned initialCapacity)
    : JSCell(vm, vm.propertyTableStructure.get())
    , m_indexSize(sizeForCapacity(initialCapacity))
    , m_indexMask(m_indexSize - 1)
    , m_index(static_cast<unsigned*>(PropertyTableMalloc::zeroedMalloc(dataSize())))
    , m_keyCount(0)
    , m_deletedCount(0)
{
    ASSERT(isPowerOf2(m_indexSize));
}

PropertyTable::PropertyTable(VM& vm, const PropertyTable& other)
    : JSCell(vm, vm.propertyTableStructure.get())
    , m_indexSize(other.m_indexSize)
    , m_indexMask(other.m_indexMask)
    , m_index(static_cast<unsigned*>(PropertyTableMalloc::malloc(dataSize())))
    , m_keyCount(other.m_keyCount)
    , m_deletedCount(other.m_deletedCount)
{
    ASSERT(isPowerOf2(m_indexSize));

    memcpy(m_index, other.m_index, dataSize());

    iterator end = this->end();
    for (iterator iter = begin(); iter != end; ++iter)
        iter->key->ref();

    // Copy the m_deletedOffsets vector.
    Vector<PropertyOffset>* otherDeletedOffsets = other.m_deletedOffsets.get();
    if (otherDeletedOffsets)
        m_deletedOffsets = makeUnique<Vector<PropertyOffset>>(*otherDeletedOffsets);
}

PropertyTable::PropertyTable(VM& vm, unsigned initialCapacity, const PropertyTable& other)
    : JSCell(vm, vm.propertyTableStructure.get())
    , m_indexSize(sizeForCapacity(initialCapacity))
    , m_indexMask(m_indexSize - 1)
    , m_index(static_cast<unsigned*>(PropertyTableMalloc::zeroedMalloc(dataSize())))
    , m_keyCount(0)
    , m_deletedCount(0)
{
    ASSERT(isPowerOf2(m_indexSize));
    ASSERT(initialCapacity >= other.m_keyCount);

    const_iterator end = other.end();
    for (const_iterator iter = other.begin(); iter != end; ++iter) {
        ASSERT(canInsert());
        reinsert(*iter);
        iter->key->ref();
    }

    // Copy the m_deletedOffsets vector.
    Vector<PropertyOffset>* otherDeletedOffsets = other.m_deletedOffsets.get();
    if (otherDeletedOffsets)
        m_deletedOffsets = makeUnique<Vector<PropertyOffset>>(*otherDeletedOffsets);
}

void PropertyTable::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    vm.heap.reportExtraMemoryAllocated(dataSize());
}

template<typename Visitor>
void PropertyTable::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    auto* thisObject = jsCast<PropertyTable*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(cell, visitor);
    visitor.reportExtraMemoryVisited(thisObject->dataSize());
}

DEFINE_VISIT_CHILDREN(PropertyTable);

void PropertyTable::destroy(JSCell* cell)
{
    static_cast<PropertyTable*>(cell)->PropertyTable::~PropertyTable();
}

PropertyTable::~PropertyTable()
{
    iterator end = this->end();
    for (iterator iter = begin(); iter != end; ++iter)
        iter->key->deref();

    PropertyTableMalloc::free(m_index);

}

} // namespace JSC

