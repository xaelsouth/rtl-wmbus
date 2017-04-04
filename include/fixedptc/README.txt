fixedptc library - a simple fixed point math header library for C.
Copyright (c) 2010-2012. Ivan Voras <ivoras@freebsd.org>
Released under the BSDL.

fixedptc is intended to be simple to use and integrate in other simple
programs, thus is it implemented as a C header library. However, as
functions in this mode of operation are all inlined, it can result in a
significant increase in code size for the final executable. If the complex
functions are used often in the end-program, the library should be
refactored into a "normal" linkable object library.
