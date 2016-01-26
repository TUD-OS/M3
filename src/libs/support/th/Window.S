
    .global     _WindowOverflow4
    .global _WindowUnderflow4
    .global     _WindowOverflow8
    .global _WindowUnderflow8
    .global     _WindowOverflow12
    .global _WindowUnderflow12

    .section .WindowVectors.text, "ax"

    .align 64
_WindowOverflow4:
    // for overflow, the window is rotated to the frame that should be saved. thus, we can simply
    // use a0 to save the a0 of that frame. rfwo rotated its back.
    s32e    a0, a5, -16             // save a0 to call[j+1]'s stack frame
    s32e    a1, a5, -12             // save a1 to call[j+1]'s stack frame
    s32e    a2, a5, -8              // save a2 to call[j+1]'s stack frame
    s32e    a3, a5, -4              // save a3 to call[j+1]'s stack frame
    rfwo                            // rotates back to call[i] position and clears windowstart[windowbase]

    .size   _WindowOverflow4, .-_WindowOverflow4

    .align 64
_WindowUnderflow4:
    // for underflow, the window is rotated to the frame to which we're about to return. so, we
    // can use a0 to load the a0 of that frame as well. rfwu rotates it back, because we'll repeat
    // the return-instruction that caused the underflow.
    l32e    a0, a5, -16             // restore a0 from call[i+1]'s stack frame
    l32e    a1, a5, -12             // restore a1 from call[i+1]'s stack frame
    l32e    a2, a5, -8              // restore a2 from call[i+1]'s stack frame
    l32e    a3, a5, -4              // restore a3 from call[i+1]'s stack frame
    rfwu

    .size   _WindowUnderflow4, .-_WindowUnderflow4

    .align 64
_WindowOverflow8:
    s32e    a0, a9, -16             // save a0 to call[j+1]'s stack frame
    l32e    a0, a1, -12             // a0 <- call[j-1]'s sp
    s32e    a1, a9, -12             // save a1 to call[j+1]'s stack frame
    s32e    a2, a9, -8              // save a2 to call[j+1]'s stack frame
    s32e    a3, a9, -4              // save a3 to call[j+1]'s stack frame
    s32e    a4, a0, -32             // save a4 to call[j]'s stack frame
    s32e    a5, a0, -28             // save a5 to call[j]'s stack frame
    s32e    a6, a0, -24             // save a6 to call[j]'s stack frame
    s32e    a7, a0, -20             // save a7 to call[j]'s stack frame
    rfwo                            // rotates back to call[i] position and clears windowstart[windowbase]

    .size   _WindowOverflow8, .-_WindowOverflow8

    .align 64
_WindowUnderflow8:
    l32e    a0, a9, -16             // restore a0 from call[i+1]'s stack frame
    l32e    a1, a9, -12             // restore a1 from call[i+1]'s stack frame
    l32e    a2, a9, -8              // restore a2 from call[i+1]'s stack frame
    l32e    a7, a1, -12             // a7 <- call[i-1]'s sp
    l32e    a3, a9, -4              // restore a3 from call[i+1]'s stack frame
    l32e    a4, a7, -32             // restore a4 from call[i]'s stack frame
    l32e    a5, a7, -28             // restore a5 from call[i]'s stack frame
    l32e    a6, a7, -24             // restore a6 from call[i]'s stack frame
    l32e    a7, a7, -20             // restore a7 from call[i]'s stack frame
    rfwu

    .size   _WindowUnderflow8, .-_WindowUnderflow8

    .align 64
_WindowOverflow12:
    s32e    a0, a13, -16            // save a0 to call[j+1]'s stack frame
    l32e    a0, a1, -12             // a0 <- call[j-1]'s sp
    s32e    a1, a13, -12            // save a1 to call[j+1]'s stack frame
    s32e    a2, a13, -8             // save a2 to call[j+1]'s stack frame
    s32e    a3, a13, -4             // save a3 to call[j+1]'s stack frame
    s32e    a4, a0, -48             // save a4 to call[j]'s stack frame
    s32e    a5, a0, -44             // save a5 to call[j]'s stack frame
    s32e    a6, a0, -40             // save a6 to call[j]'s stack frame
    s32e    a7, a0, -36             // save a7 to call[j]'s stack frame
    s32e    a8, a0, -32             // save a8 to call[j]'s stack frame
    s32e    a9, a0, -28             // save a9 to call[j]'s stack frame
    s32e    a10, a0, -24            // save a10 to call[j]'s stack frame
    s32e    a11, a0, -20            // save a11 to call[j]'s stack frame
    rfwo                            // rotates back to call[i] position and clears windowstart[windowbase]

    .size   _WindowOverflow12, .-_WindowOverflow12

    .align 64
_WindowUnderflow12:
    l32e    a0, a13, -16            // restore a0 from call[i+1]'s stack frame
    l32e    a1, a13, -12            // restore a1 from call[i+1]'s stack frame
    l32e    a2, a13, -8             // restore a2 from call[i+1]'s stack frame
    l32e    a11, a1, -12            // a11 <- call[i-1]'s sp
    l32e    a3, a13, -4             // restore a3 from call[i+1]'s stack frame
    l32e    a4, a11, -48            // restore a4 from call[i]'s stack frame
    l32e    a5, a11, -44            // restore a5 from call[i]'s stack frame
    l32e    a6, a11, -40            // restore a6 from call[i]'s stack frame
    l32e    a7, a11, -36            // restore a7 from call[i]'s stack frame
    l32e    a8, a11, -32            // restore a8 from call[i]'s stack frame
    l32e    a9, a11, -28            // restore a9 from call[i]'s stack frame
    l32e    a10, a11, -24           // restore a10 from call[i]'s stack frame
    l32e    a11, a11, -20           // restore a11 from call[i]'s stack frame
    rfwu

    .size   _WindowUnderflow12, .-_WindowUnderflow12
