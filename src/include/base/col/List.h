/*
 * Copyright (C) 2015, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of M3 (Microkernel-based SysteM for Heterogeneous Manycores).
 *
 * M3 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * M3 is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#pragma once

#include <base/Common.h>

namespace m3 {

template<class T>
class SList;
template<class T>
class DList;
template<class T,class It>
class ListIteratorBase;
class DListItem;

/**
 * A listitem for the singly linked list. It is intended that you inherit from this class to add
 * data to the item.
 */
class SListItem {
    template<class T>
    friend class SList;
    template<class T,class It>
    friend class ListIteratorBase;
    friend class DListItem;

public:
    /**
     * Constructor
     */
    explicit SListItem() : _next() {
    }

    void init() {
        _next = nullptr;
    }

private:
    SListItem *next() {
        return _next;
    }
    void next(SListItem *i) {
        _next = i;
    }

    SListItem *_next;
};

/**
 * A listitem for the doubly linked list. It is intended that you inherit from this class to add
 * data to the item.
 */
class DListItem : public SListItem {
    template<class T>
    friend class SList;
    template<class T>
    friend class DList;
    template<class T, class It>
    friend class ListIteratorBase;
    friend class NodeAllocator;

public:
    /**
     * Constructor
     */
    explicit DListItem() : SListItem(), _prev() {
    }

    void init() {
        SListItem::init();
        _prev = nullptr;
    }

private:
    DListItem *next() {
        return static_cast<DListItem*>(SListItem::next());
    }
    void next(DListItem *i) {
        SListItem::next(i);
    }
    DListItem *prev() {
        return _prev;
    }
    void prev(DListItem *i) {
        _prev = i;
    }

    DListItem *_prev;
};

/**
 * Generic iterator for a linked list. Expects the list node class to have a next() method.
 */
template<class T,class It>
class ListIteratorBase {
    template<class T1>
    friend class SList;
    template<
        template<class T1>
        class LIST,
        class T2
    >
    friend class IList;

public:
    explicit ListIteratorBase(T *n = nullptr) : _n(n) {
    }

    It& operator++() {
        _n = static_cast<T*>(_n->next());
        return static_cast<It&>(*this);
    }
    It operator++(int) {
        It tmp(static_cast<It&>(*this));
        operator++();
        return tmp;
    }
    bool operator==(const It& rhs) const {
        return _n == rhs._n;
    }
    bool operator!=(const It& rhs) const {
        return _n != rhs._n;
    }

protected:
    T *_n;
};

/**
 * Default list iterator
 */
template<class T>
class ListIterator : public ListIteratorBase<T,ListIterator<T>> {
public:
    explicit ListIterator(T *n = nullptr)
            : ListIteratorBase<T,ListIterator<T>>(n) {
    }

    T & operator*() const {
        return *this->_n;
    }
    T *operator->() const {
        return &operator*();
    }
};

/**
 * Default const list iterator
 */
template<class T>
class ListConstIterator : public ListIteratorBase<T,ListConstIterator<T>> {
public:
    explicit ListConstIterator(T *n = nullptr)
            : ListIteratorBase<T,ListConstIterator<T>>(n) {
    }

    const T & operator*() const {
        return *this->_n;
    }
    const T *operator->() const {
        return &operator*();
    }
};

}
