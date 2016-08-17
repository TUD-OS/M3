/**
* Copyright (C) 2015, René Küttner <rene.kuettner@.tu-dresden.de>
* Economic rights: Technische Universität Dresden (Germany)
*
* This file is part of M3 (Microkernel for Minimalist Manycores).
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

#include <m3/stream/Standard.h>

int main(int argc, char **argv)
{
    unsigned int counter = 0;
    char *name = argv[0];

    m3::cout << "Counter program started...\n";

    // this program simply counts and prints a message at every step

    while (1) {
        m3::cout << "Message " << counter << " from " << name << "\n";
        ++counter;
    }

    return 0;
}
