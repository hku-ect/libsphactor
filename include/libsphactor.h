/*  =========================================================================

    Copyright (c) the Contributors as noted in the AUTHORS file.

    This file is part of Sphactor, an open-source framework for high level
    actor model concurrency --- http://sphactor.org

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/

#ifndef LIBSPHACTOR_H_H_INCLUDED
#define LIBSPHACTOR_H_H_INCLUDED

//  Include the project library file
#include "sphactor_library.h"

//  Add your own public definitions here, if you need them

// structure to pass to the zactor
typedef struct {
    sphactor_handler_fn *handler;  // our handler
    void* args;       // arguments for the handler
    zuuid_t* uuid;    // uuid for the actor (NULL for auto generated)
    const char* name; // name for the actor (NULL for auto generated)
} sphactor_shim_t;

typedef struct  {
    sphactor_handler_fn *handler;
    zconfig_t *capability;
    sphactor_constructor_fn *constructor;
    void *constructor_args; // can be NULL
} sphactor_funcs_t;

#endif
