/*  =========================================================================
    sphactor_event - class description

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
    sphactor_event -
@discuss
@end
*/

#include "sphactor_classes.h"

//  Structure of our class

struct _sphactor_event_t {
    zmsg_t *msg;     //  our message payload
    const char *type;
    const char *name;
    const char *uuid;
};


//  --------------------------------------------------------------------------
//  Create a new sphactor_event

sphactor_event_t *
sphactor_event_new (zmsg_t *msg, const char *type, const char *name, const char *uuid)
{
    sphactor_event_t *self = (sphactor_event_t *) zmalloc (sizeof (sphactor_event_t));
    assert (self);
    //  Initialize class properties here
    self->type = type;
    self->name = name;
    self->uuid = uuid;
    self->msg = msg;
    return self;
}

const char *
sphactor_event_type(sphactor_event_t *self)
{
    return self->type;
}

const char *
sphactor_event_name(sphactor_event_t *self)
{
    return self->name;
}

const char *
sphactor_event_uuid(sphactor_event_t *self)
{
    return self->uuid;
}

zmsg_t*
sphactor_event_msg(sphactor_event_t *self)
{
    return self->msg;
}

//  --------------------------------------------------------------------------
//  Destroy the sphactor_event

void
sphactor_event_destroy (sphactor_event_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        sphactor_event_t *self = *self_p;
        //  Free class properties here
        //  Free object itself
        free (self);
        *self_p = NULL;
    }
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
sphactor_event_test (bool verbose)
{
    printf (" * sphactor_event: ");

    //  @selftest
    //  Simple create/destroy test
    sphactor_event_t *self = sphactor_event_new (zmsg_new(), "RECV", "name", "BLABLABLA");
    assert (self);
    assert( streq( sphactor_event_name(self) , "name" ) );
    assert( streq( sphactor_event_type(self), "RECV") );
    assert( streq( sphactor_event_uuid(self), "BLABLABLA") );
    assert( zmsg_size( sphactor_event_msg(self) ) == 0 );
    sphactor_event_destroy (&self);
    //  @end
    printf ("OK\n");
}
