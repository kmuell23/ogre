/*
-----------------------------------------------------------------------------
This source file is part of OGRE
(Object-oriented Graphics Rendering Engine)
For the latest info, see http://www.ogre3d.org/

Copyright (c) 2000-2014 Torus Knot Software Ltd

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
-----------------------------------------------------------------------------
*/
module Ogre.Core:SearchOps;

import <cstdint>;

/* Our simplified data entry structure */

struct _finddata_t
{
    char *name;
    int attrib;
    unsigned long size;
};

enum {
_A_NORMAL = 0x00,  /* Normalfile-Noread/writerestrictions */
_A_RDONLY = 0x01,  /* Read only file */
_A_HIDDEN = 0x02,  /* Hidden file */
_A_SYSTEM = 0x04,  /* System file */
_A_ARCH =   0x20  /* Archive file */
};

enum {
_A_SUBDIR = 0x10  /* Subdirectory */
};

auto _findfirst(const char *pattern, struct _finddata_t *data) -> intptr_t;

auto _findnext(intptr_t id, struct _finddata_t *data) -> int;

auto _findclose(intptr_t id) -> int;
