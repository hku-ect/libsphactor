/*  =========================================================================
    sphactor_report - class description

    Copyright (c) the Contributors as noted in the AUTHORS file.

    This file is part of Sphactor, an open-source framework for high level
    actor model concurrency --- http://sphactor.org

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/

/*
@header
    sphactor_report -
@discuss
@end
*/

#include "sphactor_classes.h"

//  Structure of our class

struct _sphactor_report_t {
    int status;             //  Status, constants in header, i.e. 0=INIT, 1=IDLE, 2=STOP, 3=DESTROY, 4=SOCK, 5=TIME, 6=FDSOCK, 7=API
    uint64_t iterations;    //  Number of iterations performed
    int64_t  recv_time;     //  time of last receive on socket
    int64_t  send_time;     //  time of last send on socket
    zosc_t *custom;         //  Optional custom OSC message
};


//  --------------------------------------------------------------------------
//  Create a new sphactor_report

sphactor_report_t *
sphactor_report_new (void)
{
    sphactor_report_t *self = (sphactor_report_t *) zmalloc (sizeof (sphactor_report_t));
    assert (self);
    //  Initialize class properties here
    self->status = 0;
    self->iterations = 0;
    self->recv_time = 0;
    self->send_time = 0;
    self->custom = NULL;
    return self;
}

sphactor_report_t *
sphactor_report_construct (int status, uint64_t iterations, int64_t recv_time, int64_t send_time, zosc_t *custom)
{
    sphactor_report_t *self = (sphactor_report_t *) zmalloc (sizeof (sphactor_report_t));
    assert (self);
    //  Initialize class properties here
    self->status = status;
    self->iterations = iterations;
    self->recv_time = recv_time;
    self->send_time = send_time;
    self->custom = custom;
    return self;
}

//  return the status in the report
int
sphactor_report_status (sphactor_report_t *self)
{
    assert(self);
    return self->status;
}

//  return the number of iterations in the report
uint64_t
sphactor_report_iterations (sphactor_report_t *self)
{
    assert( self );
    return self->iterations;
}

//  Return the time of the last send.
//  Returns 0 if it has never sent anything or isn't able to.
int64_t
sphactor_report_send_time (sphactor_report_t *self)
{
    assert(self);
    return self->send_time;
}

//  Return the time of the last receive
//  Returns 0 if it has never sent anything or isn't able to.
int64_t
sphactor_report_recv_time (sphactor_report_t *self)
{
    assert(self);
    return self->recv_time;
}

//  return the custom status as an OSC message
zosc_t *
sphactor_report_custom (sphactor_report_t *self)
{
    assert( self );
    return self->custom;
}

//  set the status in the report
void
sphactor_report_set_status (sphactor_report_t *self, int status)
{
    assert( self );
    self->status = status;
}

//  set the number of iterations in the report
void
sphactor_report_set_iterations (sphactor_report_t *self, uint64_t iterations)
{
    assert( self );
    self->iterations = iterations;
}

//  Set the time of the last send
void
sphactor_report_set_send_time (sphactor_report_t *self, int64_t send_time)
{
    assert(self);
    self->send_time = send_time;
}

//  Set the time of the last receive
void
sphactor_report_set_recv_time (sphactor_report_t *self, int64_t recv_time)
{
    assert(self);
    self->recv_time = recv_time;
}

//  set the custom status as an OSC message
void
sphactor_report_set_custom (sphactor_report_t *self, zosc_t *message)
{
    assert( self );
    if ( self->custom )
        zosc_destroy( &self->custom );
    self->custom = message;
}

//  --------------------------------------------------------------------------
//  Destroy the sphactor_report

void
sphactor_report_destroy (sphactor_report_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        sphactor_report_t *self = *self_p;
        //  Free class properties here
        self->status = 3;
        if ( self->custom )
            zosc_destroy( &self->custom );
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
sphactor_report_test (bool verbose)
{
    printf (" * sphactor_report: ");

    //  @selftest
    //  Simple create/destroy test
    sphactor_report_t *self = sphactor_report_new ();
    assert(self);
    sphactor_report_set_status( self, 99 );
    assert( sphactor_report_status(self) == 99 );
    sphactor_report_set_iterations( self, 999 );
    assert( sphactor_report_iterations(self) == 999 );
    sphactor_report_set_recv_time(self, 333 );
    assert( sphactor_report_recv_time(self) == 333 );
    sphactor_report_set_send_time(self, 444 );
    assert( sphactor_report_send_time(self) == 444 );
    // Todo test custom message
    sphactor_report_destroy (&self);

    sphactor_report_t *constr = sphactor_report_construct( 7, 77, 777, 7777, NULL);
    assert(constr);
    assert( sphactor_report_status( constr ) == 7 );
    assert( sphactor_report_iterations( constr ) == 77 );
    assert( sphactor_report_recv_time( constr ) == 777 );
    assert( sphactor_report_send_time( constr ) == 7777 );
    assert( sphactor_report_custom( constr ) == NULL );
    sphactor_report_destroy ( &constr );
    //  @end
    printf ("OK\n");
}
