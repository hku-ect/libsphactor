/*  =========================================================================
    sphactor_actor - class description

    Copyright (c) the Contributors as noted in the AUTHORS file.

    This file is part of Zyre, an open-source framework for proximity-based
    peer-to-peer applications -- See http://zyre.org.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/

#ifndef SPHACTOR_ACTOR_H_INCLUDED
#define SPHACTOR_ACTOR_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

//  @warning THE FOLLOWING @INTERFACE BLOCK IS AUTO-GENERATED BY ZPROJECT
//  @warning Please edit the model at "api/sphactor_actor.api" to make changes.
//  @interface
//  This is a stable class, and may not change except for emergencies. It
//  is provided in stable builds.
//  Constructor, creates a new Sphactor_actor instance.
SPHACTOR_EXPORT sphactor_actor_t *
    sphactor_actor_new (zsock_t *pipe, void *arg);

//  Destructor, destroys a Sphactor_actor.
SPHACTOR_EXPORT void
    sphactor_actor_destroy (sphactor_actor_t **self_p);

//  Return our sphactor's UUID string, after successful initialization
SPHACTOR_EXPORT int
    sphactor_actor_start (sphactor_actor_t *self);

//  Stop the sphactor_actor
SPHACTOR_EXPORT int
    sphactor_actor_stop (sphactor_actor_t *self);

//  Connect this sphactor_actor to another. Returns 0 on success -1
//  on failure
//
//  Note: sphactor_actor methods can only be called from within its instance!
SPHACTOR_EXPORT int
    sphactor_actor_connect (sphactor_actor_t *self, const char *dest);

//  Disconnect this sphactor_actor to another. Returns 0 on success -1
//  on failure
//
//  Note: sphactor_actor methods can only be called from within its instance!
SPHACTOR_EXPORT int
    sphactor_actor_disconnect (sphactor_actor_t *self, const char *dest);

//  Return our sphactor_actor's UUID string
//
//  Note: sphactor_actor methods can only be called from within its instance!
SPHACTOR_EXPORT zuuid_t *
    sphactor_actor_uuid (sphactor_actor_t *self);

//  Return our sphactor_actor's name. First 6 characters of the UUID by default.
//
//  Note: sphactor_actor methods can only be called from within its instance!
SPHACTOR_EXPORT const char *
    sphactor_actor_name (sphactor_actor_t *self);

//  Set the timeout for the polling of the sphactor_actor.
//
//  Note: sphactor_actor methods can only be called from within its instance!
SPHACTOR_EXPORT void
    sphactor_actor_set_timeout (sphactor_actor_t *self, int64_t timeout);

//  Adds a file descriptor to our poller (wraps zpoller_add).
//
//  Note: sphactor_actor methods can only be called from within its instance!
SPHACTOR_EXPORT int
    sphactor_actor_poller_add (sphactor_actor_t *self, void *sockfd);

//  Removes a file descriptor from our poller (wraps zpoller_remove).
//
//  Note: sphactor_actor methods can only be called from within its instance!
SPHACTOR_EXPORT int
    sphactor_actor_poller_remove (sphactor_actor_t *self, void *sockfd);

//  Return the capability of the actor as a zconfig instance.
SPHACTOR_EXPORT const zconfig_t *
    sphactor_actor_capability (sphactor_actor_t *self);

//  Set the capability of the actor as a zconfig instance. This should
//  only be done on init and can only be done once!
//  Returns 0 on success, -1 if capability is already set.
SPHACTOR_EXPORT int
    sphactor_actor_set_capability (sphactor_actor_t *self, zconfig_t *capability);

//  Sets the status report. This is a very specific threadsafe lockfree method
//  to enable the controlling thread to get this actor's status.
SPHACTOR_EXPORT void
    sphactor_actor_atomic_set_report (sphactor_actor_t *self, sphactor_report_t *report);

//  Gets the status report. This is a very specific threadsafe lockfree method
//  to enable the controlling thread to get this actor's status.
SPHACTOR_EXPORT sphactor_report_t *
    sphactor_actor_atomic_report (sphactor_actor_t *self);

//  Sets the actor's osc message for future reports. Use this in
//  handler functions to store data for use in rendering gui.
SPHACTOR_EXPORT void
    sphactor_actor_set_custom_report_data (sphactor_actor_t *self, zosc_t *message);

//  Run a single iteration of the actor. This method is only for advanced use, e.g. running an actor
//  alongside something else.
SPHACTOR_EXPORT int
    sphactor_actor_run_once (sphactor_actor_t *self);

//  Self test of this class.
SPHACTOR_EXPORT void
    sphactor_actor_test (bool verbose);

//  @end

#ifdef __cplusplus
}
#endif

#endif
