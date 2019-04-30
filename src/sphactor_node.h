/*  =========================================================================
    sphactor_node -

    Copyright (c) the Contributors as noted in the AUTHORS file.

    This file is part of Zyre, an open-source framework for proximity-based
    peer-to-peer applications -- See http://zyre.org.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/

#ifndef SPHACTOR_NODE_H_INCLUDED
#define SPHACTOR_NODE_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _sphactor_node_t sphactor_node_t;

//  @interface
//  Create new sphactor_node actor instance.
//  @TODO: Describe the purpose of this actor!
//
//      zactor_t *sphactor_node = zactor_new (sphactor_node, NULL);
//
//  Destroy sphactor_node instance.
//
//      zactor_destroy (&sphactor_node);
//
//  Enable verbose logging of commands and activity:
//
//      zstr_send (sphactor_node, "VERBOSE");
//
//  Start sphactor_node actor.
//
//      zstr_sendx (sphactor_node, "START", NULL);
//
//  Stop sphactor_node actor.
//
//      zstr_sendx (sphactor_node, "STOP", NULL);
//
//  This is the sphactor_node constructor as a zactor_fn;
SPHACTOR_EXPORT void
    sphactor_node_actor (zsock_t *pipe, void *args);

//  Self test of this actor
SPHACTOR_EXPORT void
    sphactor_node_test (bool verbose);
//  @end

#ifdef __cplusplus
}
#endif

#endif
