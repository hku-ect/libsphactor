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
    zsock_t *pipe;                //  Actor command pipe
    zpoller_t *poller;            //  Socket poller
    bool terminated;              //  Did caller ask us to quit?
    bool verbose;                 //  Verbose logging enabled?
    //  Declare properties
    zsock_t     *pub;             //  Our publisch socket
    zsock_t     *sub;             //  Our subscribe socket
    char        *endpoint;        //  Our endpoint string (based on uuid)
    zuuid_t     *uuid;            //  Our UUID identifier
    char        *name;            //  Our name (defaults to first 6 chars of our uuid)
    zhash_t     *subs;            //  a list of our subscription sockets
    zloop_t     *loop;            //  perhaps we'll use zloop instead of poller
    sphactor_handler_fn *handler; //  the handler to call on events
    void        *handler_args     //  the arguments to the handler
//    sphactor_shim_t *shim;
};


//  forward decl
zsock_t *
sphactor_node_require_transport(sphactor_node_t *self, const char *dest);

//  --------------------------------------------------------------------------
//  Create a new sphactor_node instance

static sphactor_node_t *
sphactor_node_new (zsock_t *pipe, void *args)
{
    sphactor_node_t *self = (sphactor_node_t *) zmalloc (sizeof (sphactor_node_t));
    assert (self);

    sphactor_shim_t *shim = (sphactor_shim_t *)args;
    assert( shim );
    self->handler = shim->handler;
    self->handler_args = shim->args;

    // TODO can we pass the uuid?
    self->uuid = NULL;
    if ( self->uuid == NULL)
    {
        self->uuid = zuuid_new ();
    }
    //  TODO can we pass the name?
    //  Default name for node is first 6 characters of UUID:
    //  the shorter string is more readable in logs
    self->name = (char *) zmalloc (7);
    memcpy (self->name, zuuid_str (self->uuid), 6);

    // setup a pub socket
    self->endpoint = (char *)malloc( (9 + strlen(zuuid_str(self->uuid) ) )  * sizeof(char) );
    sprintf( self->endpoint, "inproc://%s", zuuid_str(self->uuid) );
    self->pub = zsock_new( ZMQ_PUB );
    assert(self->pub);
    int rc = zsock_bind( self->pub, "%s", self->endpoint );
    assert( rc == 0);

    self->sub = zsock_new( ZMQ_SUB );
    assert(self->sub);
    // don't filter messages
    zsock_set_subscribe( self->sub, "");

    // create an empty list for our subscriptions
    self->subs = zhash_new();
    assert(self->subs);

    self->pipe = pipe;
    self->terminated = false;
    self->poller = zpoller_new (self->pipe, NULL);
    rc = zpoller_add(self->poller, self->sub);
    assert ( rc == 0 );

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

        zpoller_destroy (&self->poller);
        zuuid_destroy(&self->uuid);
        zstr_free(&self->name);
        zstr_free(&self->endpoint);
        zsock_destroy(&self->pub);
        zsock_destroy(&self->sub);
        // iterate subs list and destroy
        zsock_t *itr = zhash_first( self->subs );
        while (itr)
        {
            zsock_destroy( &itr );
            itr = zhash_next( self->subs );
        }
        zhash_destroy(&self->subs);

        //  Free object itself
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
sphactor_node_connect (sphactor_node_t *self, const char *dest)
{
    assert ( self);
    assert ( dest );
    assert( streq(dest, self->endpoint) == 0 );  //  endpoint should not be ours
    int rc = zsock_connect(self->sub, "%s", dest);
    assert(rc == 0);
    return rc;
}


//  Disconnect this sphactor_node from another. Destination is an endpoint string
//  Returns 0 on success -1 on failure

static int
sphactor_node_disconnect (sphactor_node_t *self, char *dest)
{
    assert (self);
    //  request sub socket to disconnect, this will create the socket if it doesn't exist
    zsock_t *sub = sphactor_node_require_transport (self, dest);
    assert ( sub );
    int rc = zsock_disconnect (sub, "%s", dest);
    assert ( rc == 0 );
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
    if (self->verbose ) zsys_info("command: %s", command);
    if (streq (command, "START"))
        sphactor_node_start (self);
    else
    if (streq (command, "STOP"))
        sphactor_node_stop (self);
    else
    if (streq (command, "CONNECT"))
    {
        char *dest = zmsg_popstr (request);
        int rc = sphactor_node_connect (self, dest);
        assert( rc == 0 );
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
        zstr_send ( self->pipe, self->name );
    else
    if (streq (command, "ENDPOINT"))
        zstr_send ( self->pipe, self->endpoint );
    else
    if (streq (command, "SEND"))
    {
        if (zmsg_size(request) > 0 )
        {
            zmsg_send(&request, self->pub);
        }
        else
            zstr_send ( self->pub, self->name );
    }
    else
    if (streq (command, "TRIGGER"))     //  trigger the node to run its callback
    {
        zmsg_t *retmsg = self->handler(NULL, self->handler_args);
        if (retmsg)
        {
            //publish it
            zmsg_send(&retmsg, self->pub);
        }
        zmsg_destroy( &retmsg );
    }
    else
    if (streq (command, "SET VERBOSE"))
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

//  Based on the given endpoint return a relevant socket. If no socket available
//  it will create a new one for the transport.
//  Returns the socket or NULL if failed
zsock_t *
sphactor_node_require_transport(sphactor_node_t *self, const char *dest)
{
    // determine transport medium (split on ://)
    /* somehow this doesn't work
    const char delim[3] = "://";
    char *transport, *orig;
    orig = strdup(dest);
    transport = strtok( orig, delim);
    assert( transport );
    zsock_t *sub = (zsock_t *)zhash_lookup(self->subs, transport);
    if ( sub == NULL )
    {
        // create new socket for transport
        if (self->verbose) zsys_info("Creating new sub socket for %s transport", transport);
        sub = zsock_new_sub(dest, NULL);
        assert( sub );
        int rc = zhash_insert(self->subs, transport, sub);
        assert( rc == 0);
    }

    //zstr_free(&transport);
    zstr_free(&orig);
    */
    return self->sub;
}

//  a producer and consumer method for testing the sphactor.

static zmsg_t *
sph_actor_producer(zmsg_t *msg, void *args)
{
    static int count = 0;
    if ( !msg )
    {
        msg = zmsg_new();
        zmsg_addstr(msg, "PING");
        zmsg_addstrf(msg, "%d", count+=1);
        zsys_info("producer sent PING %d", count );
    }
    return msg;
}

static zmsg_t *
sph_actor_consumer(zmsg_t *msg, void *args)
{
    assert( msg );
    char *cmd = zmsg_popstr( msg );
    assert( streq(cmd, "PING") );
    char *count = zmsg_popstr( msg );
    assert( strlen(count) == 1 );
    zsys_info("consumer received %s %s", cmd, count );
    return NULL;
}

//  --------------------------------------------------------------------------
//  This is the actor which runs in its own thread.

void
sphactor_node_actor (zsock_t *pipe, void *args)
{
    if ( !args )
    {
        // as a test for now
        sphactor_shim_t consumer = { &sph_actor_consumer, NULL };
        args = (void *)&consumer;
    }
    sphactor_node_t *self = sphactor_node_new (pipe, args);
    if (!self)
        return;          //  Interrupted

    //  Signal actor successfully initiated
    zsock_signal (self->pipe, 0);

    while (!self->terminated) {
        zsock_t *which = (zsock_t *) zpoller_wait (self->poller, -1);
        if (which == self->pipe)
            sphactor_node_recv_api (self);
        //  if a sub socket then process actor
        else
        if ( which == self->sub ) {
            zmsg_t *msg = zmsg_recv(which);
            if (!msg)
            {
                break; //  interrupted
            }
            // TODO: think this through
            zmsg_t *retmsg = self->handler(msg, self->handler_args);
            if (retmsg)
            {
                // publish the msg
                zmsg_send(&retmsg, self->pub);
            }
        }
        zsock_t *sub = zhash_first( self->subs );
        while ( sub )
        {
            if ( which == sub )
            {
                //  run the actor's handle
                //self->handler
                break;
            }
        }

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
    sphactor_shim_t consumer = { &sph_actor_consumer, NULL };
    zactor_t *sphactor_node = zactor_new (sphactor_node_actor, &consumer);
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

    // acquire the endpoint
    zstr_send(sphactor_node, "ENDPOINT");
    char *endpoint = zstr_recv(sphactor_node);

    // send something through the pub socket  and receive it
    zsock_t *sub = zsock_new_sub(endpoint, "");
    assert(sub);
    zstr_send(sphactor_node, "SEND");
    char *name3 = zstr_recv(sub);
    assert( streq ( name, name3 ));

    //  send a ping to the consumer
    zsock_t *pub = zsock_new_pub("inproc://bla");
    assert( pub);
    zstr_sendm(sphactor_node, "CONNECT");
    zstr_send(sphactor_node, "inproc://bla");
    zclock_sleep(10);
    zstr_sendm(pub, "PING");
    zstr_send(pub, "1");
    zclock_sleep(10);   //  prevent destroy before ping being handled

    // create a producer actor
    sphactor_shim_t producer = { &sph_actor_producer, NULL };
    zactor_t *sphactor_producer = zactor_new (sphactor_node_actor, &producer);
    assert (sphactor_producer);
    // get endpoint of producer
    zstr_send(sphactor_producer, "ENDPOINT");
    char *prod_endpoint = zstr_recv(sphactor_producer);
    // connect producer to consumer
    zstr_sendm(sphactor_node, "CONNECT");
    zstr_send(sphactor_node, prod_endpoint);
    zsys_info("%s %s %s", zstr_recv(sphactor_node), zstr_recv(sphactor_node), zstr_recv(sphactor_node));
    zstr_send(sphactor_producer, "TRIGGER");
    zclock_sleep(10);   //  prevent destroy before ping being handled


    zsock_destroy(&sub);
    zsock_destroy(&pub);
    zuuid_destroy(&uuid);
    zstr_free(&name2);
    zactor_destroy (&sphactor_node);
    zactor_destroy (&sphactor_producer);
    //  @end

    printf ("OK\n");
}
