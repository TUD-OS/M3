/*
 * Copyright (C) 2016-2018, Nils Asmussen <nils@os.inf.tu-dresden.de>
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

#include <base/col/List.h>
#include <assert.h>

namespace m3 {

/**
 * The singly linked list. Takes an arbitrary class as list-item and expects it to have a prev(),
 * prev(T*), next() and next(*T) method. In most cases, you should inherit from SListItem and
 * specify your class for T.
 */
template<class T>
class SList {
public:
    using iterator          = ListIterator<T>;
    using const_iterator    = ListConstIterator<T>;

    /**
     * Constructor. Creates an empty list
     */
    explicit SList()
        : _head(nullptr),
          _tail(nullptr),
          _len(0) {
    }

    /**
     * Move-constructor
     */
    SList(SList<T> &&l)
        : _head(l._head),
          _tail(l._tail),
          _len(l._len) {
        l._head = nullptr;
        l._tail = nullptr;
        l._len = 0;
    }

    /**
     * @return the number of items in the list
     */
    size_t length() const {
        return _len;
    }

    /**
     * @return beginning of list (you can change the list items)
     */
    iterator begin() {
        return iterator(_head);
    }
    /**
     * @return end of list
     */
    iterator end() {
        return iterator();
    }
    /**
     * @return tail of the list, i.e. the last valid item
     */
    iterator tail() {
        return iterator(_tail);
    }

    /**
     * @return beginning of list (you can NOT change the list items)
     */
    const_iterator begin() const {
        return const_iterator(_head);
    }
    /**
     * @return end of list
     */
    const_iterator end() const {
        return const_iterator();
    }
    /**
     * @return tail of the list, i.e. the last valid item (NOT changeable)
     */
    const_iterator tail() const {
        return const_iterator(_tail);
    }

    /**
     * Appends the given item to the list. This works in constant time.
     *
     * @param e the list item
     * @return the position where it has been inserted
     */
    iterator append(T *e) {
        if(_head == nullptr)
            _head = e;
        else
            _tail->next(e);
        _tail = e;
        e->next(nullptr);
        _len++;
        return iterator(e);
    }
    /**
     * Inserts the given item into the list after <p>. This works in constant time.
     *
     * @param p the previous item (nullptr = insert it at the beginning)
     * @param e the list item
     * @return the position where it has been inserted
     */
    iterator insert(T *p, T *e) {
        e->next(p ? p->next() : _head);
        if(p)
            p->next(e);
        else
            _head = e;
        if(!e->next())
            _tail = e;
        _len++;
        return iterator(e);
    }
    /**
     * Removes the first item from the list
     *
     * @return the removed item (or 0 if there is none)
     */
    T *remove_first() {
        if(_len == 0)
            return 0;
        T *res = _head;
        _head = static_cast<T*>(_head->next());
        if(_head == 0)
            _tail = 0;
        _len--;
        return res;
    }
    /**
     * Removes the first item from the list for which <pred> returns true. This works in linear time.
     * Does NOT expect that the item is in the list!
     *
     * @param pred the predicate
     * @return the removed item or nullptr
     */
    template<typename P>
    T *remove_if(P pred) {
        T *t = _head, *p = nullptr;
        while(t && !pred(t)) {
            p = t;
            t = static_cast<T*>(t->next());
        }
        if(!t)
            return nullptr;
        remove(p, t);
        return t;
    }
    /**
     * Removes the given item from the list. This works in linear time.
     * Does NOT expect that the item is in the list!
     *
     * @param e the list item
     * @return true if the item has been found and removed
     */
    bool remove(T *e) {
        return remove_if([e](T *e2) { return e == e2; }) != nullptr;
    }
    /**
     * Removes <e> from the list, assuming that <p> is its predecessor.
     *
     * @param p the previous item
     * @param e the item to remove
     */
    void remove(T *p, T *e) {
        if(p)
            p->next(e->next());
        else
            _head = static_cast<T*>(e->next());
        if(!e->next())
            _tail = p;
        _len--;
    }
    /**
     * Removes all items from the list
     */
    void remove_all() {
        _head = nullptr;
        _tail = nullptr;
        _len = 0;
    }

private:
    T *_head;
    T *_tail;
    size_t _len;
};

}
