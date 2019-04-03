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

/*
@header
    sphactor -
@discuss
@end
*/

#include "sphactor_classes.h"

//  Structure of our class

struct _sphactor_t {
    zactor_t *actor;            //  A Sphactor instance wraps a zactor
    char    *name;              //  Copy of our node's name
    char    *uuid;              //  Copy of our node's uuid
    char    *endpoint;          //  Copy of our node's endpoint
    zlist_t *subscriptions;     //  Copy of our node's (incoming) connections
};


//  --------------------------------------------------------------------------
//  Create a new sphactor. Pass a name and uuid. If your specify NULL
//  a uuid will be generated and the first ./src/libsph.pc.in6 chars will be used as a name

sphactor_t *
sphactor_new (const char *name, zuuid_t *uuid)
{
    sphactor_t *self = (sphactor_t *) zmalloc (sizeof (sphactor_t));
    assert (self);

    self->actor = zactor_new( sphactor_node_actor, uuid);

    if (name)
    {
        sphactor_set_name( self, name );
    }

    return self;
}


//  --------------------------------------------------------------------------
//  Destroy the sphactor

void
sphactor_destroy (sphactor_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        sphactor_t *self = *self_p;
        //  Free class properties here
        //  Free object itself
        free (self);
        *self_p = NULL;
    }
}

const char *
sphactor_uuid (sphactor_t *self)
{
    assert(self);
    if (self->uuid == NULL )
    {
        // I'm not sure if this is safe if there's already a queue
        // on the actor's pipe???
        zstr_send(self->actor, "UUID");
        self->uuid = zstr_recv( self->actor );
    }
    return self->uuid;
}

const char *
sphactor_name (sphactor_t *self)
{
    assert(self);
    if ( self->name == NULL )
    {
        zstr_send(self->actor, "NAME");
        self->name = zstr_recv( self->actor );
    }
    return self->name;
}

void
sphactor_set_name (sphactor_t *self, const char *name)
{
    assert (self);
    assert (name);
    zstr_sendx (self->actor, "SET NAME", name, NULL);
}

void
sphactor_set_verbose (sphactor_t *self, bool on)
{
    assert (self);
    zstr_sendm (self->actor, "SET VERBOSE");
    zstr_sendf( self->actor, "%d", on);
}


//  --------------------------------------------------------------------------
//  Self test of this class

// If your selftest reads SCMed fixture data, please keep it in
// src/selftest-ro; if your test creates filesystem objects, please
// do so under src/selftest-rw.
// The following pattern is suggested for C selftest code:
//    char *filename = NULL;
//    filename = zsys_sprintf ("%s/%s", SELFTEST_DIR_RO, "mytemplate.file");
//    assert (filename);
//    ... use the "filename" for I/O ...
//    zstr_free (&filename);
// This way the same "filename" variable can be reused for many subtests.
#define SELFTEST_DIR_RO "src/selftest-ro"
#define SELFTEST_DIR_RW "src/selftest-rw"

void
sphactor_test (bool verbose)
{
    printf (" * sphactor: ");

    //  @selftest
    //  Simple create/destroy test
    sphactor_t *self = sphactor_new (NULL, NULL);
    assert (self);
    sphactor_destroy (&self);
    //  @end
    printf ("OK\n");
}
