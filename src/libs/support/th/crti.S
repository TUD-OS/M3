.section .init
.global _init
_init:
    entry   a1, 64

.section .fini
.global _fini
_fini:
    entry   a1, 64
