/*  =========================================================================

    Copyright (c) the Contributors as noted in the AUTHORS file.

    This file is part of Sphactor, an open-source framework for high level
    actor model concurrency --- http://sphactor.org

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
typedef struct _sphactor_event_t{
    zmsg_t *msg;  // msg received on the socket
    const char  *type;  // type of event (API or SOC)
    const char  *name;  // name of the actor
    const char  *uuid;  // uuid of the actor
    const sphactor_actor_t  *actor;   // name of the actor
} sphactor_event_t;

//  @warning THE FOLLOWING @INTERFACE BLOCK IS AUTO-GENERATED BY ZPROJECT
//  @warning Please edit the model at "api/sphactor.api" to make changes.
//  @interface
//  This is a stable class, and may not change except for emergencies. It
//  is provided in stable builds.
// Callback function for socket activity
typedef zmsg_t * (sphactor_handler_fn) (
    sphactor_event_t *event, void *arg);

// Callback function to create actor instances
typedef void * (sphactor_constructor_fn) (
    void *arg);

//  Constructor, creates a new Sphactor instance.
SPHACTOR_EXPORT sphactor_t *
    sphactor_new (sphactor_handler_fn handler, void *arg, const char *name, zuuid_t *uuid);

//  Constructor, creates a new Sphactor instance by its typename.
SPHACTOR_EXPORT sphactor_t *
    sphactor_new_by_type (const char *actor_type, const char *name, zuuid_t *uuid);

//  Constructor, create an actor from the provided configuration
SPHACTOR_EXPORT sphactor_t *
    sphactor_load (const zconfig_t *config);

//  Destructor, destroys a Sphactor instance.
SPHACTOR_EXPORT void
    sphactor_destroy (sphactor_t **self_p);

//  Register an actor type, this is used to dynamically create actors. It needs to know the
//  handler method, a method for constructing the actor and the arguments to construct the
//  actor.
//  Returns 0 on success, -1 if already registered.
SPHACTOR_EXPORT int
    sphactor_register (const char *actor_type, sphactor_handler_fn handler, sphactor_constructor_fn constructor, void *constructor_arg);

//
SPHACTOR_EXPORT int
    sphactor_unregister (const char *actor_type);

//
SPHACTOR_EXPORT zlist_t *
    sphactor_get_registered (void);

//
SPHACTOR_EXPORT void
    sphactor_dispose (void);

//  Return our sphactor's UUID string, after successful initialization
SPHACTOR_EXPORT zuuid_t *
    sphactor_ask_uuid (sphactor_t *self);

//  Return our sphactor's name, after successful initialization. First 6
//  characters of the UUID by default.
SPHACTOR_EXPORT const char *
    sphactor_ask_name (sphactor_t *self);

//  Return our sphactor's type, after successful initialization.
//  NULL by default.
SPHACTOR_EXPORT const char *
    sphactor_ask_actor_type (sphactor_t *self);

//  Return our sphactor's endpoint, after successful initialization.
//  The endpoint is usually inproc://{uuid}
SPHACTOR_EXPORT const char *
    sphactor_ask_endpoint (sphactor_t *self);

//  Set the public name of this Sphactor instance overriding the default.
SPHACTOR_EXPORT void
    sphactor_ask_set_name (sphactor_t *self, const char *name);

//  Set the actor's type.
SPHACTOR_EXPORT void
    sphactor_ask_set_actor_type (sphactor_t *self, const char *actor_type);

//  Set the timeout of this Sphactor's actor. This is used for the timeout
//  of the poller so the sphactor actor is looped for a fixed interval. Note
//  that the sphactor's actor method receives a NULL message if it is
//  triggered by timeout event as opposed to when triggered by a socket
//  event. By default the timeout is -1 implying it never timeouts.
SPHACTOR_EXPORT void
    sphactor_ask_set_timeout (sphactor_t *self, int64_t timeout);

//  Return the current timeout of this sphactor actor's poller. By default
//  the timeout is -1 which means it never times out but only triggers
//  on socket events.
SPHACTOR_EXPORT int64_t
    sphactor_ask_timeout (sphactor_t *self);

//  Connect the actor's sub socket to a pub endpoint. Returns 0 if succesful -1 on
//  failure.
SPHACTOR_EXPORT int
    sphactor_ask_connect (sphactor_t *self, const char *endpoint);

//  Disconnect the actor's sub socket from a pub endpoint. Returns 0 if succesful -1 on
//  failure.
SPHACTOR_EXPORT int
    sphactor_ask_disconnect (sphactor_t *self, const char *endpoint);

//  Return the list of connections of this actor.
SPHACTOR_EXPORT zlist_t *
    sphactor_connections (sphactor_t *self);

//  Return socket for talking to the actor and for polling.
SPHACTOR_EXPORT zsock_t *
    sphactor_socket (sphactor_t *self);

//  Set the verbosity of the actor.
SPHACTOR_EXPORT void
    sphactor_ask_set_verbose (sphactor_t *self, bool on);

//  Set the reporting of the actor. Reporting is used for communicating
//  status reports safely. For example this can be used to render a
//  visual representation of the actor.
SPHACTOR_EXPORT void
    sphactor_ask_set_reporting (sphactor_t *self, bool on);

//  Request the actor's capability. This consists of a zconfig string
//  containing its capability parameters.
SPHACTOR_EXPORT zconfig_t *
    sphactor_ask_capability (sphactor_t *self);

//  Do an API request to the running actor. (TODO perhaps make this variadic)
//  Returns 0 if send succesfully.
SPHACTOR_EXPORT int
    sphactor_ask_api (sphactor_t *self, const char *api_call, const char *api_format, const char *value);

//  Set the stage position of the actor.
SPHACTOR_EXPORT void
    sphactor_set_position (sphactor_t *self, float x, float y);

//  Return the X position of the actor.
SPHACTOR_EXPORT float
    sphactor_position_x (sphactor_t *self);

//  Return the Y position of the actor.
SPHACTOR_EXPORT float
    sphactor_position_y (sphactor_t *self);

//  Create new zconfig
SPHACTOR_EXPORT zconfig_t *
    sphactor_zconfig_new (const char *filename);

//  Append to zconfig
SPHACTOR_EXPORT zconfig_t *
    sphactor_zconfig_append (sphactor_t *self, zconfig_t *root);

//  Gets the current status report from the actor.
//  A NULL pointer is returned if reporting is disabled.
SPHACTOR_EXPORT sphactor_report_t *
    sphactor_report (sphactor_t *self);

//  Self test of this class.
SPHACTOR_EXPORT void
    sphactor_test (bool verbose);

//  @end

#ifdef __cplusplus
}
#endif

#endif
