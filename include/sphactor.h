/*  =========================================================================
    sphactor - class description

    Copyright (c) the Contributors as noted in the AUTHORS file.

    This file is part of Zyre, an open-source framework for proximity-based
    peer-to-peer applications -- See http://zyre.org.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/

#ifndef SPHACTOR_H_INCLUDED
#define SPHACTOR_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

//  sphactor event type is received by the handlers (perhaps we'll make this into a zproject class)
typedef struct {
    zmsg_t *msg;  // msg received on the socket
    const char  *type;  // type of event (API or SOC)
    const char  *name;  // name of the node
    const char  *uuid;  // uuid of the node
} sphactor_event_t;

//  @warning THE FOLLOWING @INTERFACE BLOCK IS AUTO-GENERATED BY ZPROJECT
//  @warning Please edit the model at "api/sphactor.api" to make changes.
//  @interface
//  This is a stable class, and may not change except for emergencies. It
//  is provided in stable builds.
// Callback function for socket activity
typedef zmsg_t * (sphactor_handler_fn) (
    sphactor_event_t *event, void *arg);

//  Constructor, creates a new Sphactor node.
SPHACTOR_EXPORT sphactor_t *
    sphactor_new (sphactor_handler_fn handler, void *arg, const char *name, zuuid_t *uuid);

//  Destructor, destroys a Sphactor node.
SPHACTOR_EXPORT void
    sphactor_destroy (sphactor_t **self_p);

//
SPHACTOR_EXPORT int
    sphactor_sph_register (const char *typename, sphactor_handler_fn handler);

//
SPHACTOR_EXPORT int
    sphactor_sph_unregister (const char *typename);

//  Return our sphactor's UUID string, after successful initialization
SPHACTOR_EXPORT zuuid_t *
    sphactor_uuid (sphactor_t *self);

//  Return our sphactor's name, after successful initialization. First 6
//  characters of the UUID by default.
SPHACTOR_EXPORT const char *
    sphactor_name (sphactor_t *self);

//  Return our sphactor's endpoint, after successful initialization.
//  The endpoint is usually inproc://{uuid}
SPHACTOR_EXPORT const char *
    sphactor_endpoint (sphactor_t *self);

//  Set the public name of this sphactor node overriding the default.
SPHACTOR_EXPORT void
    sphactor_set_name (sphactor_t *self, const char *name);

//  Set the timeout of this sphactor node. This is used for the timeout
//  of the poller so the sphactor is looped for a fixed interval. Note
//  that the sphactor's method receives a NULL message if it is
//  triggered by timeout event as opposed to when triggered by a socket
//  event. By default the timeout is -1 implying it never timeouts.
SPHACTOR_EXPORT void
    sphactor_set_timeout (sphactor_t *self, int64_t timeout);

//  Return the current timeout of this sphactor node's poller. By default
//  the timeout is -1 which means it never times out but only triggers
//  on socket events.
SPHACTOR_EXPORT int64_t
    sphactor_get_timeout (sphactor_t *self);

//  Connect the node's sub socket to a pub endpoint. Returns 0 if succesful -1 on
//  failure.
SPHACTOR_EXPORT int
    sphactor_connect (sphactor_t *self, const char *endpoint);

//  Disconnect the node's sub socket from a pub endpoint. Returns 0 if succesful -1 on
//  failure.
SPHACTOR_EXPORT int
    sphactor_disconnect (sphactor_t *self, const char *endpoint);

//  Return socket for talking to the sphactor node and for polling.
SPHACTOR_EXPORT zsock_t *
    sphactor_socket (sphactor_t *self);

//  Set the verbosity of the node.
SPHACTOR_EXPORT void
    sphactor_set_verbose (sphactor_t *self, bool on);

//  Create new zconfig
SPHACTOR_EXPORT zconfig_t *
    sphactor_zconfig_new (const char *filename);

//  Append to zconfig
SPHACTOR_EXPORT zconfig_t *
    sphactor_zconfig_append (sphactor_t *self, zconfig_t *root);

//  Self test of this class.
SPHACTOR_EXPORT void
    sphactor_test (bool verbose);

//  @end

#ifdef __cplusplus
}
#endif

#endif
