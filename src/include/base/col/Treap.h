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
#include <base/stream/OStream.h>
#include <assert.h>

namespace m3 {

/**
 * A node in the treap. You may create a subclass of this to add data to your nodes.
 */
template<typename N, typename KEY>
class TreapNode {
    template<class T>
    friend class Treap;
public:
    typedef KEY key_t;

    /**
     * Constructor
     *
     * @param key the key of the node
     */
    explicit TreapNode(key_t key) : _key(key), _prio(), _left(), _right() {
    }

    /**
     * @return true if the given key matches this one
     */
    bool matches(key_t key) {
        return _key == key;
    }

    /**
     * @return the key
     */
    key_t key() const {
        return _key;
    }
    /**
     * Sets the key. Note that changing the key while this node is already inserted in the tree
     * won't work in general.
     *
     * @param key the new key
     */
    void key(key_t key) {
        _key = key;
    }

    /**
     * Prints this node into <os>
     *
     * @param os the ostream
     */
    void print(OStream &os) const {
        os << _key;
    }

private:
    key_t _key;
    int _prio;
    N *_left;
    N *_right;
};

/**
 * A treap is a combination of a binary tree and a heap. So the child-node on the left has a
 * smaller key than the parent and the child on the right has a bigger key.
 * Additionally the root-node has the smallest priority and it increases when walking towards the
 * leafs. The priority is "randomized" by fibonacci-hashing. This way, the tree is well balanced
 * in most cases.
 *
 * The idea and parts of the implementation are taken from the MMIX simulator, written by
 * Donald Knuth (http://mmix.cs.hm.edu/)
 */
template<class T>
class Treap {
public:
    /**
     * Creates an empty treap
     */
    explicit Treap() : _prio(314159265), _root(nullptr) {
    }

    /**
     * @return true if the treap is empty
     */
    bool empty() const {
        return _root == nullptr;
    }

    /**
     * Clears the tree (does not free nodes)
     */
    void clear() {
        _prio = 314159265;
        _root = nullptr;
    }

    /**
     * Finds the node with given key in the tree
     *
     * @param key the key
     * @return the node or nullptr if not found
     */
    T *find(typename T::key_t key) const {
        for(T *p = _root; p != nullptr; ) {
            if(p->matches(key))
                return static_cast<T*>(p);
            if(key < p->key())
                p = p->_left;
            else
                p = p->_right;
        }
        return nullptr;
    }

    /**
     * Inserts the given node in the tree. Note that it is expected, that the key of the node is
     * already set.
     *
     * @param node the node to insert
     */
    void insert(T *node) {
        // we want to insert it by priority, so find the first node that has <= priority
        T **q, *p;
        for(p = _root, q = &_root; p && p->_prio < _prio; p = *q) {
            if(node->key() < p->key())
                q = &p->_left;
            else
                q = &p->_right;
        }

        *q = node;
        // fibonacci hashing to spread the priorities very even in the 32-bit room
        node->_prio = _prio;
        _prio += 0x9e3779b9;    // floor(2^32 / phi), with phi = golden ratio

        // At this point we want to split the binary search tree p into two parts based on the
        // given key, forming the left and right subtrees of the new node q. The effect will be
        // as if key had been inserted before all of p’s nodes.
        T **l = &(*q)->_left, **r = &(*q)->_right;
        while(p) {
            if(node->key() < p->key()) {
                *r = p;
                r = &p->_left;
                p = *r;
            }
            else {
                *l = p;
                l = &p->_right;
                p = *l;
            }
        }
        *l = *r = nullptr;
    }

    /**
     * Removes the given node from the tree.
     *
     * @param node the node to remove (DOES have to be a valid pointer)
     */
    void remove(T *node) {
        T **p;
        // find the position where reg is stored
        for(p = &_root; *p && *p != node; ) {
            if(node->key() < (*p)->key())
                p = &(*p)->_left;
            else
                p = &(*p)->_right;
        }
        assert(*p == node);
        remove_from(p, node);
    }

    /**
     * Removes the root node of the tree and returns it.
     *
     * @return the root node
     */
    T *remove_root() {
        T *res = _root;
        if(res)
            remove(res);
        return res;
    }

    /**
     * Prints this treap into the given ostream
     *
     * @param os the ostream
     * @param tree whether to print it as a tree
     */
    void print(OStream &os, bool tree = true) const {
        if(_root)
            printRec(os, _root, 0, tree);
    }

private:
    Treap(const Treap&);
    Treap& operator=(const Treap&);

    void remove_from(T **p, T *node) {
        // two childs
        if(node->_left && node->_right) {
            // rotate with left
            if(node->_left->_prio < node->_right->_prio) {
                T *t = node->_left;
                node->_left = t->_right;
                t->_right = node;
                *p = t;
                remove_from(&t->_right, node);
            }
            // rotate with right
            else {
                T *t = node->_right;
                node->_right = t->_left;
                t->_left = node;
                *p = t;
                remove_from(&t->_left, node);
            }
        }
        // one child: replace us with our child
        else if(node->_left)
            *p = node->_left;
        else if(node->_right)
            *p = node->_right;
        // no child: simply remove us from parent
        else
            *p = nullptr;
    }

    void printRec(OStream &os, T *n, int layer, bool tree) const {
        n->print(os);
        os << "\n";
        if(n->_left) {
            if(tree)
                os << fmt("", layer * 2) << "  |-(l) ";
            printRec(os, n->_left, layer + 1, tree);
        }
        if(n->_right) {
            if(tree)
                os << fmt("", layer * 2) << "  |-(r) ";
            printRec(os, n->_right, layer + 1, tree);
        }
    }

    int _prio;
    T *_root;
};

}
