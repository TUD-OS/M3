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

#include <base/util/Util.h>

namespace m3 {

namespace impl {

template<class IT, class CMP>
IT divide(IT ileft, IT ipiv, CMP comp) {
    IT i = ileft;
    IT j = ipiv - 1;

    do {
        // right until the element is > piv
        while(!comp(*ipiv, *i) && i < ipiv)
            ++i;
        // left until the element is < piv
        while(!comp(*j, *ipiv) && j > ileft)
            --j;

        // swap
        if(i < j)
            Util::swap(*i, *j);
    }
    while(i < j);

    // swap piv with element i
    if(comp(*ipiv, *i))
        Util::swap(*i, *ipiv);
    return i;
}

template<class IT, class CMP>
void qsort(IT left, IT right, CMP comp) {
    // TODO someday we should provide a better implementation which uses another sort-algo
    // for small arrays, don't uses recursion and so on
    if(left < right) {
        IT i = divide(left, right, comp);
        qsort(left, i - 1, comp);
        qsort(i + 1, right, comp);
    }
}

}

/**
 * Sorts the elements in the range [<first> .. <last>) into ascending order.
 * The elements are compared using operator< for the first version, and <comp> for the second.
 * Elements that would compare equal to each other are not guaranteed to keep their original
 * relative order.
 *
 * @param first the start-position (inclusive)
 * @param last the end-position (exclusive)
 * @param comp the compare-"function"
 */
template<class IT, class CMP>
void sort(IT first, IT last, CMP comp) {
    impl::qsort(first, last - 1, comp);
}

}
