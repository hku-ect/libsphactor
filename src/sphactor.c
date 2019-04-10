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
    zuuid_t *uuid;              //  Copy of our node's uuid
    char    *endpoint;          //  Copy of our node's endpoint
    zlist_t *subscriptions;     //  Copy of our node's (incoming) connections
};


//  --------------------------------------------------------------------------
//  Create a new sphactor. Pass a name and uuid. If your specify NULL
//  a uuid will be generated and the first 6 chars will be used as a name

sphactor_t *
sphactor_new (const char *name, zuuid_t *uuid)
{
    sphactor_t *self = (sphactor_t *) zmalloc (sizeof (sphactor_t));
    assert (self);

    if (uuid)
        self->uuid = zuuid_dup(uuid);

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
        zactor_destroy (&self->actor);
        zstr_free( &self->name);
        if (self->uuid) zuuid_destroy(&self->uuid);
        zstr_free(&self->endpoint);
        //  Free object itself

        free (self);
        *self_p = NULL;
    }
}

zuuid_t *
sphactor_uuid (sphactor_t *self)
{
    assert(self);
    if (self->uuid == NULL )
    {
        // I'm not sure if this is safe if there's already a queue
        // on the actor's pipe???
        zstr_send(self->actor, "UUID");
        self->uuid = zuuid_new(); // or just malloc it?
        int rc = zsock_recv( self->actor, "U", &self->uuid );
        assert ( rc==0 );
        assert ( self->uuid );
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

const char *
sphactor_endpoint (sphactor_t *self)
{
    assert(self);
    if ( self->endpoint == NULL )
    {
        zstr_send(self->actor, "ENDPOINT");
        self->endpoint = zstr_recv( self->actor );
    }
    return self->endpoint;
}

int
sphactor_connect (sphactor_t *self, const char *endpoint)
{
    assert(self);
    assert(endpoint);
    zstr_sendx( self->actor, "CONNECT", endpoint, NULL );
    zmsg_t *response = zmsg_recv( self->actor );
    char *cmd = zmsg_popstr( response );
    assert( streq( cmd, "CONNECTED"));
    char *dest = zmsg_popstr(response);
    char *rc = zmsg_popstr(response);
    assert ( streq(rc, "0") );
    int rci = streq(rc, "0") ? 0 : -1;
    zstr_free(&cmd);
    zstr_free(&dest);
    zstr_free(&rc);

    return rci;}

int
sphactor_disconnect (sphactor_t *self, const char *endpoint)
{
    assert(self);
    assert(endpoint);
    int rc = zstr_sendx( self->actor, "DISCONNECT", endpoint, NULL );
    assert( rc == 0);
    zmsg_t *response = zmsg_recv( self->actor );
    char *cmd = zmsg_popstr( response );
    assert( streq( cmd, "DISCONNECTED"));
    char *dest = zmsg_popstr(response);
    char *rcc = zmsg_popstr(response);
    assert ( streq(rcc, "0") );
    int rci = streq(rcc, "0") ? 0 : -1;
    zstr_free(&cmd);
    zstr_free(&dest);
    zstr_free(&rcc);

    return rci;
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
    //  Simple create/destroy/name/uuid test
    sphactor_t *self = sphactor_new ( NULL, NULL);
    assert (self);
    zuuid_t *uuidtest = sphactor_uuid(self);
    assert(uuidtest);
    //assert( zuuid_eq(uuid, zuuid_data(uuid2) ) );
    //  name should be the first 6 chars from the uuid
    const char *name = sphactor_name( self );
    char *name2 = (char *) zmalloc (7);
    memcpy (name2, zuuid_str(uuidtest), 6);
    assert( streq ( name, name2 ));
    zstr_free(&name2);

    sphactor_destroy (&self);

    //  Simple create/destroy/name/uuid test with specified uuid
    zuuid_t *uuid = zuuid_new();
    self = sphactor_new ( NULL, uuid);
    assert (self);
    uuidtest = sphactor_uuid(self);
    assert(uuidtest);
    assert( zuuid_eq(uuid, zuuid_data(uuidtest) ) );
    //  name should be the first 6 chars from the uuid
    name = sphactor_name( self );
    name2 = (char *) zmalloc (7);
    memcpy (name2, zuuid_str(uuid), 6);
    assert( streq ( name, name2 ));
    zstr_free(&name2);
    sphactor_destroy (&self);

    //  Simple create/destroy/connect/disconnect test
    sphactor_t *pub = sphactor_new ( NULL, NULL);
    sphactor_t *sub = sphactor_new ( NULL, NULL);
    assert (pub);
    assert (sub);
    sphactor_set_verbose(sub, true);
    //  get endpoints
    const char *pendp = sphactor_endpoint(pub);
    const char *sendp = sphactor_endpoint(sub);
    zuuid_t *puuid = sphactor_uuid(pub);
    zuuid_t *suuid = sphactor_uuid(sub);
    char *endpointest = (char *)malloc( (9 + strlen(zuuid_str(puuid) ) )  * sizeof(char) );
    sprintf( endpointest, "inproc://%s", zuuid_str(puuid));
    assert( streq( pendp, endpointest));
    sprintf( endpointest, "inproc://%s", zuuid_str(suuid));
    assert( streq( sendp, endpointest));
    //  connect sub to pub
    int rc = sphactor_connect(sub, pendp);
    assert( rc == 0);
    //  disconnect sub to pub
    rc = sphactor_disconnect(sub, pendp);
    assert( rc == 0);

    zstr_free( &endpointest );
    sphactor_destroy (&pub);
    sphactor_destroy (&sub);

    //  @end
    printf ("OK\n");
}
