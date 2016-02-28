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

#ifndef RCTMUX_ARCH_T3_H
#define RCTMUX_ARCH_T3_H

#include <xtensa/xtruntime.h>

#define REGSPILL_AREA_SIZE (XCHAL_NUM_AREGS * sizeof(word_t))
#define EPC_REG (21)

EXTERN_C void _start();

void arch_setup();
bool arch_init();
bool arch_save_state();
bool arch_restore_state();
void arch_finalize();
void arch_wipe_mem();
void arch_idle_mode();

#endif /* RCTMUX_ARCH_T3_H */
