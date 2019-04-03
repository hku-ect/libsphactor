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

/*
@header
    sphactor_node -
@discuss
@end
*/

#include "sphactor_classes.h"

//  Structure of our actor

struct _sphactor_node_t {
    zsock_t *pipe;              //  Actor command pipe
    zpoller_t *poller;          //  Socket poller
    bool terminated;            //  Did caller ask us to quit?
    bool verbose;               //  Verbose logging enabled?
    //  Declare properties
    zsock_t     *pub;           //  Our publisch socket
    char        *endpoint;      //  Our endpoint string (based on uuid)
    zuuid_t     *uuid;          //  Our UUID identifier
    char        *name;          //  Our name (defaults to first 6 chars of our uuid)
    zlist_t     *subs;          //  a list of our subscription sockets
    zloop_t     *loop;          // perhaps we'll use zloop instead of poller
};


//  --------------------------------------------------------------------------
//  Create a new sphactor_node instance

static sphactor_node_t *
sphactor_node_new (zsock_t *pipe, void *args)
{
    sphactor_node_t *self = (sphactor_node_t *) zmalloc (sizeof (sphactor_node_t));
    assert (self);

    self->uuid = (zuuid_t *)args;
    if (self->uuid == NULL)
    {
        self->uuid = zuuid_new ();
    }
    //  Default name for node is first 6 characters of UUID:
    //  the shorter string is more readable in logs
    self->name = (char *) zmalloc (7);
    memcpy (self->name, zuuid_str (self->uuid), 6);

    // setup a pub socket
    self->endpoint = (char *)malloc( (9 + strlen(zuuid_str(self->uuid) ) )  * sizeof(char) );
    sprintf( self->endpoint, "inproc://%s", zuuid_str(self->uuid) );
    self->pub = zsock_new_pub(self->endpoint);
    assert(self->pub);

    // create an empty list for our subscriptions
    self->subs = zlist_new();
    assert(self->subs);

    self->pipe = pipe;
    self->terminated = false;
    self->poller = zpoller_new (self->pipe, NULL);

    return self;
}


//  --------------------------------------------------------------------------
//  Destroy the sphactor_node instance

static void
sphactor_node_destroy (sphactor_node_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        sphactor_node_t *self = *self_p;

        //  TODO: Free actor properties
        zuuid_destroy(&self->uuid);
        zstr_free(&self->name);
        zstr_free(&self->endpoint);
        zsock_destroy(&self->pub);
        // iterate subs list and destroy
        zsock_t *itr = zlist_first( self->subs );
        while (itr)
        {
            zsock_destroy( &itr );
            itr = zlist_next( self->subs );
        }

        //  Free object itself
        zpoller_destroy (&self->poller);
        free (self);
        *self_p = NULL;
    }
}


//  Start this actor. Return a value greater or equal to zero if initialization
//  was successful. Otherwise -1.

static int
sphactor_node_start (sphactor_node_t *self)
{
    assert (self);

    //  TODO: Add startup actions

    return 0;
}


//  Stop this actor. Return a value greater or equal to zero if stopping
//  was successful. Otherwise -1.

static int
sphactor_node_stop (sphactor_node_t *self)
{
    assert (self);

    //  TODO: Add shutdown actions

    return 0;
}

//  Connect this sphactor_node to another
//  Returns 0 on success -1 on failure

static int
sphactor_node_connect (sphactor_node_t *self, char *dest)
{
    assert (self);

    //TODO
    return 0;
}


//  Disconnect this sphactor_node from another. Destination is an endpoint string
//  Returns 0 on success -1 on failure

static int
sphactor_node_disconnect (sphactor_node_t *self, char *dest)
{
    assert (self);

    //  TODO: Add shutdown actions

    return 0;
}
//  Here we handle incoming message from the node

static void
sphactor_node_recv_api (sphactor_node_t *self)
{
    //  Get the whole message of the pipe in one go
    zmsg_t *request = zmsg_recv (self->pipe);
    if (!request)
       return;        //  Interrupted

    char *command = zmsg_popstr (request);
    if (streq (command, "START"))
        sphactor_node_start (self);
    else
    if (streq (command, "STOP"))
        sphactor_node_stop (self);
    else
    if (streq (command, "CONNECT"))
    {
        char *dest = zmsg_popstr (request);
        int rc = sphactor_node_disconnect (self, dest);
        zstr_sendm (self->pipe, "CONNECTED");
        zstr_sendm (self->pipe, dest);
        zstr_sendf (self->pipe, "%i", rc);
    }
    else
    if (streq (command, "DISCONNECT"))
    {
        char *dest = zmsg_popstr (request);
        int rc = sphactor_node_disconnect (self, dest);
        zstr_sendm (self->pipe, "DISCONNECTED");
        zstr_sendm (self->pipe, dest);
        zstr_sendf (self->pipe, "%i", rc);
    }
    else
    if (streq (command, "UUID"))
    {
        zsock_send(self->pipe, "U", self->uuid);
    }
    else
    if (streq (command, "NAME"))
    {
        zstr_send ( self->pipe, self->name );
    }
    else
    if (streq (command, "VERBOSE"))
        self->verbose = true;
    else
    if (streq (command, "$TERM"))
        //  The $TERM command is send by zactor_destroy() method
        self->terminated = true;
    else {
        zsys_error ("invalid command '%s'", command);
        assert (false);
    }
    zstr_free (&command);
    zmsg_destroy (&request);
}


//  --------------------------------------------------------------------------
//  This is the actor which runs in its own thread.

void
sphactor_node_actor (zsock_t *pipe, void *args)
{
    sphactor_node_t * self = sphactor_node_new (pipe, args);
    if (!self)
        return;          //  Interrupted

    //  Signal actor successfully initiated
    zsock_signal (self->pipe, 0);

    while (!self->terminated) {
        zsock_t *which = (zsock_t *) zpoller_wait (self->poller, 0);
        if (which == self->pipe)
            sphactor_node_recv_api (self);
       //  Add other sockets when you need them.
    }
    sphactor_node_destroy (&self);
}

//  --------------------------------------------------------------------------
//  Self test of this actor.

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
sphactor_node_test (bool verbose)
{
    printf (" * sphactor_node: ");
    //  @selftest
    //  Simple create/destroy test
    zactor_t *sphactor_node = zactor_new (sphactor_node_actor, NULL);
    assert (sphactor_node);
    // acquire the uuid
    zstr_send(sphactor_node, "UUID");
    zuuid_t *uuid = zuuid_new();
    int rc = zsock_recv( sphactor_node, "U", &uuid );
    assert ( rc==0 );
    assert ( uuid );

    // acquire the name
    zstr_send(sphactor_node, "NAME");
    char *name = zstr_recv(sphactor_node);
    // test if the name matches the first 6 chars
    char *name2 = (char *) zmalloc (7);
    memcpy (name2, zuuid_str(uuid), 6);
    assert( streq ( name, name2 ));

    zuuid_destroy(&uuid);
    zstr_free(&name2);
    zactor_destroy (&sphactor_node);
    //  @end

    printf ("OK\n");
}
