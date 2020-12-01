/*  =========================================================================
    sphactor_actor - class description

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
    sphactor_actor -
@discuss
@end
*/

#include "sphactor_classes.h"
#if defined(__WINDOWS__)
//  Apparently in MSCV correct alignment of types is alread guaranteed
#define _Atomic(T) struct { T __val; }
#include <winnt.h>
#else
#include <stdatomic.h>
#endif

//  Structure of our class

struct _sphactor_actor_t {
    zsock_t *pipe;                //  Actor command pipe
    zpoller_t *poller;            //  Socket poller
    bool terminated;              //  Did caller ask us to quit?
    bool verbose;                 //  Verbose logging enabled?
    bool reporting;                  //  Enable reporting (sphactor_report)
    //  Declare properties
    zsock_t     *pub;             //  Our publisch socket
    zsock_t     *sub;             //  Our subscribe socket
    char        *endpoint;        //  Our endpoint string (based on uuid)
    zuuid_t     *uuid;            //  Our UUID identifier
    char        *name;            //  Our name (defaults to first 6 chars of our uuid)
    char        *actor_type;      //  Our actors typename (defaults to NULL)
    zhash_t     *subs;            //  a list of our subscription sockets
    zloop_t     *loop;            //  perhaps we'll use zloop instead of poller
    int64_t     timeout;          //  timeout to wait on polling. Indirect rate for calling the handler
    int64_t     time_next;        //  timestamp for our next iteration
    sphactor_handler_fn *handler; //  the handler to call on events
    void        *handler_args;    //  the arguments to the handler
//    sphactor_shim_t *shim;
    zhashx_t    *fd_handlers;     //  a list of handlers for external fd's
    uint64_t    iterations;       //  number of iterations (cycles) performed
    int         status;           //  sphactor_report_status constant, see sphactor_report.h
    zconfig_t   *capability;      //  The capability zconfig describing parameters (ie. for generating UI)
    zosc_t      *reportMsg;       //  the report message containing the actor's state
    _Atomic     (void*) atomic_report;  // atomic pointer to report data
};

//  --------------------------------------------------------------------------
//  Create a new sphactor_actor

sphactor_actor_t *
sphactor_actor_new (zsock_t *pipe, void *args)
{
    sphactor_actor_t *self = (sphactor_actor_t *) zmalloc (sizeof (sphactor_actor_t));
    assert (self);

    sphactor_shim_t *shim = (sphactor_shim_t *)args;
    assert( shim );
    self->handler = shim->handler;
    self->handler_args = shim->args;
    self->uuid = shim->uuid;
    self->actor_type = NULL;
    self->timeout = -1;
    self->capability = NULL;
    // initialise the status report
    self->iterations = 0;
    self->status = SPHACTOR_REPORT_INIT;
    self->reportMsg = NULL;
    // don't use set_report as it will try to free random memory
#if defined(__WINDOWS__)
    InterlockedExchangePointer( &self->atomic_report, sphactor_report_construct( self->status, self->iterations, NULL ) );
#else
    atomic_store( &self->atomic_report, sphactor_report_construct( self->status, self->iterations, NULL ) );
#endif
    if ( self->uuid == NULL)
    {
        self->uuid = zuuid_new ();
    }
    //  Default name for actor is first 6 characters of UUID:
    //  the shorter string is more readable in logs
    if ( shim->name == NULL )
    {
        self->name = (char *) zmalloc (7);
        memcpy (self->name, zuuid_str (self->uuid), 6);
    }
    else {
        // If we pass a string literal, we can't free self->name on destroy...
        //  so malloc and memcpy
        self->name = (char *) zmalloc (strlen(shim->name) + 1 );
        memcpy (self->name, shim->name, strlen(shim->name));
    }

    // setup a pub socket
    self->endpoint = (char *)malloc( (10 + strlen(zuuid_str(self->uuid) ) )  * sizeof(char) );
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
    self->reporting = true;    // report by default
    self->poller = zpoller_new (self->pipe, NULL);
    rc = zpoller_add(self->poller, self->sub);
    assert ( rc == 0 );

    self->fd_handlers = zhashx_new();

    return self;
}


//  --------------------------------------------------------------------------
//  Destroy the sphactor_actor

void
sphactor_actor_destroy (sphactor_actor_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        sphactor_actor_t *self = *self_p;

        if ( self->reporting )
        {
            self->status = SPHACTOR_REPORT_DESTROY;
            sphactor_actor_atomic_set_report(self, sphactor_report_construct(self->status, self->iterations, zosc_dup(self->reportMsg)));
        }

        // signal upstream we are destroying
        sphactor_event_t ev = { NULL, "DESTROY", self->name, zuuid_str(self->uuid), self };
        if ( self->handler )
        {
            zmsg_t *retmsg = self->handler(&ev, self->handler_args);
            if (retmsg)
            {
                zmsg_destroy( &retmsg );
            }
        }
        zpoller_destroy (&self->poller);
        zuuid_destroy(&self->uuid);
        zstr_free(&self->name);
        if ( self->actor_type )
            zstr_free(&self->actor_type);
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

        zhashx_destroy(&self->fd_handlers);
        if (self->capability)
        {
            zconfig_destroy(&self->capability);
        }
        // free the atomic report
        sphactor_report_t *rep = sphactor_actor_atomic_report(self);
        if ( rep )
            sphactor_report_destroy(&rep);

        zosc_destroy(&self->reportMsg);

        //  Free object itself
        free (self);
        *self_p = NULL;
    }
}


//  Start this actor. Return a value greater or equal to zero if initialization
//  was successful. Otherwise -1.

int
sphactor_actor_start (sphactor_actor_t *self)
{
    assert (self);

    //  TODO: Add startup actions

    return 0;
}

//  Stop this actor. Return a value greater or equal to zero if stopping
//  was successful. Otherwise -1.

int
sphactor_actor_stop (sphactor_actor_t *self)
{
    assert (self);

    //  TODO: Add shutdown actions

    return 0;
}

//  Connect this sphactor_actor to another
//  Returns 0 on success -1 on failure

int
sphactor_actor_connect (sphactor_actor_t *self, const char *dest)
{
    assert ( self);
    assert ( dest );
    assert( streq(dest, self->endpoint) == 0 );  //  endpoint should not be ours
    int rc = zsock_connect(self->sub, "%s", dest);
    assert(rc == 0);
    return rc;
}


//  Disconnect this sphactor_actor from another. Destination is an endpoint string
//  Returns 0 on success -1 on failure

int
sphactor_actor_disconnect (sphactor_actor_t *self, const char *dest)
{
    assert (self);
    assert ( self->sub );
    int rc = zsock_disconnect (self->sub, "%s", dest);
    assert ( rc == 0 );
    return 0;
}

//  Return our sphactor_actor's UUID string
//
//  Note: sphactor_actor methods can only be called from within its instance!
zuuid_t *
sphactor_actor_uuid (sphactor_actor_t *self)
{
    assert(self);
    return self->uuid; // should we duplicate?
}

//  Return our sphactor_actor's name. First 6 characters of the UUID by default.
//
//  Note: sphactor_actor methods can only be called from within its instance!
const char *
sphactor_actor_name (sphactor_actor_t *self)
{
    assert(self);
    return self->name;
}

void
sphactor_actor_set_timeout (sphactor_actor_t *self, int64_t timeout)
{
    self->timeout = timeout;
    if (self->timeout >= 0 ) self->time_next = zclock_mono() + self->timeout;
    else self->time_next = INT_MAX;
}

int
sphactor_actor_poller_add (sphactor_actor_t *self, void * fd, sphactor_actor_handler_fn * handler)
{
    assert(self);
    int rc = zpoller_add(self->poller, fd);
    zhashx_insert(self->fd_handlers, fd, (void*)handler);
    assert (rc == 0);
    return rc;
}

int
sphactor_actor_poller_remove (sphactor_actor_t *self, void * fd)
{
    assert(self);
    int rc = zpoller_remove(self->poller, fd);
    zhashx_delete(self->fd_handlers, fd);
    assert (rc == 0);
    return rc;
}

const zconfig_t *
sphactor_actor_capability (sphactor_actor_t *self)
{
    assert( self );
    return self->capability;
}

int
sphactor_actor_set_capability (sphactor_actor_t *self, zconfig_t *capability)
{
    assert(self);
    assert(capability);
    if (self->capability) return -1;
    self->capability = capability;
    return 0;
}

void
sphactor_actor_atomic_set_report( sphactor_actor_t *self, sphactor_report_t *report)
{
    // swap the report pointer atomically
#if defined(__WINDOWS__)
    sphactor_report_t *prev = (sphactor_report_t *)InterlockedExchangePointer( &self->atomic_report, (void *)report);
#else
    sphactor_report_t *prev = (sphactor_report_t *)atomic_exchange( &self->atomic_report, (void *)report);
#endif
    
    // if prev is not NULL we need to destroy it
    if ( prev != (sphactor_report_t *)NULL )
    {
        // destroy the memory it is pointing to
        sphactor_report_destroy( &prev );
    }
    // TODO: for memory efficiency we could reuse the report object?
}

sphactor_report_t *
sphactor_actor_atomic_report(sphactor_actor_t *self)
{
    if ( !self->reporting ) return NULL;   // reporting is disabled
    // swap the report pointer atomically with NULL
#if defined(__WINDOWS__)
    sphactor_report_t *report = (sphactor_report_t *)InterlockedExchangePointer( &self->atomic_report, (void *)NULL );
#else
    sphactor_report_t *report = (sphactor_report_t *)atomic_exchange( &self->atomic_report, (void *)NULL );
#endif
    // we now own report so the caller must destroy when finished with it
    // unless it's null of course
    return report;
}

// Stores an osc message that becomes the report_custom
void sphactor_actor_set_custom_report_data(sphactor_actor_t *self, zosc_t* message )
{
    zosc_t *prev = self->reportMsg;
    self->reportMsg = message;
    if ( prev != NULL)
        zosc_destroy(&prev);
}

//  Here we handle incoming (API) messages from the pipe from the controller (main thread)

static void
sphactor_actor_recv_api (sphactor_actor_t *self)
{
    //  Get the whole message of the pipe in one go
    zmsg_t *request = zmsg_recv (self->pipe);
    if (!request)
       return;        //  Interrupted

    char *command = zmsg_popstr (request);
    if (self->verbose ) zsys_info("command: %s", command);
    if (streq (command, "START"))
        sphactor_actor_start (self);
    else
    if (streq (command, "STOP"))
        sphactor_actor_stop (self);
    else
    if (streq (command, "INSTANCE"))
    {
        // Danger: this method will send the 'self' pointer
        // over the pipe, internal use only!
        int rc = zsock_send( self->pipe, "p", self);
        assert( rc == 0 );
    }
    else
    if (streq (command, "CONNECT"))
    {
        char *dest = zmsg_popstr (request);
        int rc = sphactor_actor_connect (self, dest);
        assert( rc == 0 );
        zstr_sendm (self->pipe, "CONNECTED");
        zstr_sendm (self->pipe, dest);
        zstr_sendf (self->pipe, "%i", rc);
        zstr_free(&dest);
    }
    else
    if (streq (command, "DISCONNECT"))
    {
        char *dest = zmsg_popstr (request);
        int rc = sphactor_actor_disconnect (self, dest);
        zstr_sendm (self->pipe, "DISCONNECTED");
        zstr_sendm (self->pipe, dest);
        zstr_sendf (self->pipe, "%i", rc);
        zstr_free(&dest);
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
    if (streq (command, "TYPE"))
        zstr_send ( self->pipe, self->actor_type ? self->actor_type : "" );
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
    if (streq (command, "TRIGGER"))     //  trigger the actor to run its callback
    {
        sphactor_event_t ev = { NULL, "SOCK", self->name, zuuid_str(self->uuid) };
        zmsg_t *retmsg = self->handler( &ev, self->handler_args);
        if (retmsg)
        {
            //publish it
            zmsg_send(&retmsg, self->pub);
        }
        zmsg_destroy( &retmsg );
    }
    else
    if (streq (command, "SET NAME"))
    {
        //TODO: There are two cases here, if it was set from a string literal, this crashes
        //  if it was allocated, and we skip this, it leaks
        if ( self->name != NULL ) {
            zstr_free(&self->name);
        }
        self->name = zmsg_popstr(request);
        assert(self->name);
    }
    else
    if (streq (command, "SET TYPE"))
    {
        if ( self->actor_type != NULL ) {
            zstr_free(&self->actor_type);
        }
        self->actor_type = zmsg_popstr(request);
        assert(self->actor_type);
    }
    else
    if (streq (command, "SET VERBOSE"))
    {
        char *cmd = zmsg_popstr(request);
        if ( cmd )
        {
            if ( streq(cmd, "FALSE" ) )
                self->verbose = false;
            else
                self->verbose = true;
            zstr_free( &cmd );
        }
        else
            self->verbose = false;
    }
    else
    if (streq (command, "SET REPORTING" ))  // Toggle report
    {
        char *cmd = zmsg_popstr(request);
        if ( cmd )
        {
            if ( streq(cmd, "FALSE" ) )
                self->reporting = false;
            else
                self->reporting = true;
            zstr_free( &cmd );
        }
        else
            self->reporting = true;
    }
    else
    if (streq (command, "SET TIMEOUT"))
    {
        char *rate =  zmsg_popstr(request);
        //  rate is per second, timeout is in ms
        sphactor_actor_set_timeout( self, (int64_t) atol(rate) );
        zstr_free(&rate);
    }
    else
    if (streq (command, "TIMEOUT"))
        zstr_sendf(self->pipe, "%li", self->timeout);
    else
    if (streq (command, "CAPABILITY"))
    {
        //  we're sending the raw pointer as it is readonly!
        int rc = zsock_send(self->pipe, "p", self->capability);
        assert(rc == 0);
    }
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


//  a producer and consumer method for testing the sphactor.

static zmsg_t *
sph_actor_producer(sphactor_event_t *ev, void *args)
{
    if ( ev->msg == NULL ) return NULL;
    static int count = 0;
    if ( ev == NULL )
    {
        zmsg_t *msg = zmsg_new();
        zmsg_addstr(msg, "PING");
        zmsg_addstrf(msg, "%d", count+=1);
        zsys_info("producer sent PING %d", count );
        return msg;
    }
    assert( ev->msg );
    if ( zmsg_size(ev->msg) > 0 )
    {
        return ev->msg;
    }
    else
    {
        zmsg_destroy(&ev->msg);
    }
    return NULL;
}

static zmsg_t *
sph_actor_consumer(sphactor_event_t *ev, void *args)
{
    if ( ev->msg == NULL )
    {
        zsys_info("Timed event!");
        return NULL;
    }
    assert( ev->msg );
    char *cmd = zmsg_popstr( ev->msg );
    assert( streq(cmd, "PING") );
    char *count = zmsg_popstr( ev->msg );
    assert( strlen(count) == 1 );
    zsys_info("consumer received %s %s", cmd, count );
    zstr_free(&count);
    zstr_free(&cmd);
    if ( zmsg_size(ev->msg) > 0 )
    {
        return ev->msg;
    }
    else
    {
        zmsg_destroy(&ev->msg);
    }
    return NULL;
}

static zmsg_t *
sph_actor_rate_test(sphactor_event_t *ev, void *args)
{
    assert( ev->msg == NULL );
    return NULL;
}

static zmsg_t *
sph_actor_lifecycle(sphactor_event_t *ev, void *args)
{
    static int i = 0;
    zsys_info("%s: %i, %s", ev->name, i, ev->type);
    sphactor_report_t *report = sphactor_actor_atomic_report( (sphactor_actor_t *)ev->actor );
    //assert( report );
    switch ( i )
    {
    case 0 :
        assert( streq( ev->type, "INIT" ) );
        if (report ) assert( sphactor_report_status( report ) == SPHACTOR_REPORT_INIT );
        sphactor_actor_set_timeout( (sphactor_actor_t *)ev->actor, 16);
        break;
    case 1 :
        assert( streq( ev->type, "TIME") );
        if (report ) assert( sphactor_report_status( report ) == SPHACTOR_REPORT_TIME );
        // reset the timeout
        assert( ev->actor->timeout == 16 );
        sphactor_actor_set_timeout( (sphactor_actor_t *)ev->actor, -1);
        break;
    case 2 :
        assert( streq( ev->type, "STOP" ) );
        if (report ) assert( sphactor_report_status( report ) == SPHACTOR_REPORT_STOP );
        break;
    case 3 :
        assert( streq( ev->type, "DESTROY" ) );
        if (report ) assert( sphactor_report_status( report ) == SPHACTOR_REPORT_DESTROY );
        break;
    default:
        assert( false );
        break;
    }
    sphactor_report_destroy( &report );
    i++;
    assert( ev->msg == NULL );
    return NULL;
}

static zmsg_t *
sph_actor_reportertest(sphactor_event_t *ev, void *args)
{
    if ( ev->actor->verbose )
        zsys_info("%s: %s, %s", sphactor_actor_name( (sphactor_actor_t *)ev->actor), ev->name, ev->type);
    if ( streq(ev->type, "TIME" ) )
    {
        // assure our status is TIME
        assert( ev->actor->status == SPHACTOR_REPORT_TIME );
        assert( ev->msg == NULL );
    }
    return NULL;
}

//  --------------------------------------------------------------------------
//  This is the actor which runs in its own thread.

void
sphactor_actor_run (zsock_t *pipe, void *args)
{
    if ( !args )
    {
        // as a test for now
        sphactor_shim_t consumer = { &sph_actor_consumer, NULL, NULL, NULL };
        args = (void *)&consumer;
    }
    sphactor_actor_t *self = sphactor_actor_new (pipe, args);
    if (!self)
        return;          //  Interrupted

    //  Signal actor successfully initiated
    zsock_signal (self->pipe, 0);
    //  Signal handler we're initiated
    sphactor_event_t ev = { NULL, "INIT", self->name, zuuid_str(self->uuid), self };
    if ( self->handler)
    {
        zmsg_t *initretmsg = self->handler(&ev, self->handler_args);
        if (initretmsg) zmsg_destroy(&initretmsg);
    }

    // TODO: this should run on start so timed trigger always run at start
    int64_t time_till_next = self->timeout;
    self->time_next = zclock_mono() + self->timeout;
    if ( self->timeout == -1)
    {
        time_till_next = -1;
        self->time_next = INT_MAX;
    }

    while (!self->terminated) {
        //  determine poller timeout
        if ( zclock_mono() > self->time_next )
        {
            zsys_error("sphactor_actor: %s, is falling behind! Consider decreasing it's rate if this happens often", self->name);
            time_till_next = 0;
        }
        else
        {
            time_till_next = self->time_next - zclock_mono();
            // if time_till_next will be 0 the poller we return immediatelly
            // so we only set a report when that is not the case
            self->status = SPHACTOR_REPORT_IDLE;
            if ( self->reporting )
                sphactor_actor_atomic_set_report(self, sphactor_report_construct(self->status, self->iterations, zosc_dup(self->reportMsg)));
        }


        void *which = (void *) zpoller_wait (self->poller, (int)time_till_next );

        if (self->timeout > 0 ) self->time_next = zclock_mono() + self->timeout;

        if ( which != NULL && !zsock_is(which) ) {
            int * fd = (int*)which;
            sphactor_actor_handler_fn * func = zhashx_lookup(self->fd_handlers, fd);

            if ( func ) {
                //  update our status report 6=FDSOCK
                self->status = SPHACTOR_REPORT_FDSOCK;
                if ( self->reporting )
                    sphactor_actor_atomic_set_report(self, sphactor_report_construct(self->status, self->iterations, zosc_dup(self->reportMsg)));

                zmsg_t * retmsg = func( (void * )fd );
                if ( retmsg != NULL ) {
                    // publish the msg
                    zmsg_send(&retmsg, self->pub);

                    // delete message if we have no connections (otherwise it leaks)
                    if ( zsock_endpoint(self->pub) == NULL ) {
                        zmsg_destroy(&retmsg);
                    }
                }
            } //else???
        }
        else
        if (which == self->pipe)
        {
            //  update our status report 7=API
            self->status = SPHACTOR_REPORT_API;
            if ( self->reporting )
                sphactor_actor_atomic_set_report(self, sphactor_report_construct(self->status, self->iterations, zosc_dup(self->reportMsg)));
            sphactor_actor_recv_api (self);
        }
        //  if a sub socket then process actor
        else if ( which == self->sub ) {
            zmsg_t *msg = zmsg_recv(which);
            if (!msg)
            {
                break; //  interrupted
            }
            // TODO: think this through
            //  update our status report 4=SOCK
            self->status = SPHACTOR_REPORT_SOCK;
            if ( self->reporting )
                sphactor_actor_atomic_set_report(self, sphactor_report_construct(self->status, self->iterations, zosc_dup(self->reportMsg)));

            sphactor_event_t ev = { msg, "SOCK", self->name, zuuid_str(self->uuid), self };
            zmsg_t *retmsg = self->handler(&ev, self->handler_args);
            if (retmsg)
            {
                // publish the msg
                zmsg_send(&retmsg, self->pub);

                // delete message if we have no connections (otherwise it leaks)
                if ( zsock_endpoint(self->pub) == NULL )
                    zmsg_destroy(&retmsg);
            }
        }
        else if ( which == NULL ) {
            //  timed events don't carry a message instead NULL is passed
            //  update our status report 5=TIME
            self->status = SPHACTOR_REPORT_TIME;
            if ( self->reporting )
                sphactor_actor_atomic_set_report(self, sphactor_report_construct(self->status, self->iterations, zosc_dup(self->reportMsg)));

            // do we have a handler? TODO: we should never have a NULL handler???
            if ( self->handler )
            {
                sphactor_event_t ev = { NULL, "TIME", self->name, zuuid_str(self->uuid), self };
                zmsg_t *retmsg = self->handler(&ev, self->handler_args);
                if (retmsg)
                {
                    // publish the msg
                    zmsg_send(&retmsg, self->pub);

                    // delete message if we have no connections (otherwise it leaks)
                    if ( zsock_endpoint(self->pub) == NULL ) {
                        zmsg_destroy(&retmsg);
                    }
                }
            }
        }
        else {
            // TODO remove this, do we need it?
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
        self->iterations++;
    }
    // signal our handler we're stopping
    if ( self->handler)
    {
        ev.msg = NULL;
        ev.type = "STOP";
        ev.name = self->name;
        ev.uuid = zuuid_str(self->uuid);
        ev.actor = self;

        self->status = SPHACTOR_REPORT_STOP;
        if ( self->reporting )
            sphactor_actor_atomic_set_report(self, sphactor_report_construct(self->status, self->iterations, zosc_dup(self->reportMsg)));

        zmsg_t *destrretmsg = self->handler(&ev, self->handler_args);
        if (destrretmsg) zmsg_destroy(&destrretmsg);
    }
    sphactor_actor_destroy (&self);
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
sphactor_actor_test (bool verbose)
{
    printf (" * sphactor_actor: ");
    //  @selftest
    //  Simple create/destroy test
    sphactor_shim_t consumer = { &sph_actor_consumer, NULL, NULL, NULL };
    zactor_t *sphactor_actor = zactor_new (sphactor_actor_run, &consumer);
    assert (sphactor_actor);
    // acquire the uuid
    zstr_send(sphactor_actor, "UUID");
    zuuid_t *uuid;
    int rc = zsock_recv( sphactor_actor, "U", &uuid );
    assert ( rc==0 );
    assert ( uuid );

    // acquire the name
    zstr_send(sphactor_actor, "NAME");
    char *name = zstr_recv(sphactor_actor);
    // test if the name matches the first 6 chars
    char *name2 = (char *) zmalloc (7);
    memcpy (name2, zuuid_str(uuid), 6);
    assert( streq ( name, name2 ));
    zstr_free(&name);
    zstr_free(&name2);
    zuuid_destroy(&uuid);

    // set the name and acquire it
    zstr_sendm(sphactor_actor, "SET NAME");
    zstr_send(sphactor_actor, "testname");
    zstr_send(sphactor_actor, "NAME");
    char *testname = zstr_recv(sphactor_actor);
    assert( streq ( testname, "testname" ));

    // acquire the endpoint
    zstr_send(sphactor_actor, "ENDPOINT");
    char *endpoint = zstr_recv(sphactor_actor);

    // send something through the pub socket  and receive it
    // the SEND command returns the name of the actor
    zsock_t *sub = zsock_new_sub(endpoint, "");
    assert(sub);
    zstr_send(sphactor_actor, "SEND");
    char *name3 = zstr_recv(sub);
    assert( streq ( testname, name3 ));
    zstr_free(&endpoint);
    zstr_free(&testname);
    zstr_free(&name3);

    //  send a ping to the consumer
    zsock_t *pub = zsock_new_pub("inproc://bla");
    assert( pub);
    zstr_sendm(sphactor_actor, "CONNECT");
    zstr_send(sphactor_actor, "inproc://bla");
    zclock_sleep(10);
    zstr_sendm(pub, "PING");
    zstr_send(pub, "1");
    zclock_sleep(10);   //  prevent destroy before ping being handled

    // create a producer actor
    sphactor_shim_t producer = { &sph_actor_producer, NULL, NULL, NULL };
    zactor_t *sphactor_producer = zactor_new (sphactor_actor_run, &producer);
    assert (sphactor_producer);
    // get endpoint of producer
    zstr_send(sphactor_producer, "ENDPOINT");
    char *prod_endpoint = zstr_recv(sphactor_producer);
    // connect producer to consumer
    zstr_sendm(sphactor_actor, "CONNECT");
    zstr_send(sphactor_actor, prod_endpoint);
    char *msg1 = zstr_recv(sphactor_actor);
    char *msg2 = zstr_recv(sphactor_actor);
    char *msg3 = zstr_recv(sphactor_actor);
    zsys_info("%s %s %s", msg1, msg2, msg3);
    zstr_send(sphactor_producer, "TRIGGER");
    zclock_sleep(10);   //  prevent destroy before ping being handled
    zstr_free(&msg1);
    zstr_free(&msg2);
    zstr_free(&msg3);
    zstr_free(&prod_endpoint);

    zsock_destroy(&sub);
    zsock_destroy(&pub);
    zuuid_destroy(&uuid);
    zstr_free(&name2);
    zactor_destroy (&sphactor_actor);
    zactor_destroy (&sphactor_producer);

    // timeout test
    sphactor_shim_t rate_tester = { &sph_actor_rate_test, NULL, NULL, NULL };
    zactor_t *sphactor_rate_tester = zactor_new (sphactor_actor_run, &rate_tester);
    assert(sphactor_rate_tester);
    zstr_send(sphactor_rate_tester, "TIMEOUT");
    char *ret = zstr_recv(sphactor_rate_tester);
    assert( streq( ret, "-1") );
    zstr_sendm(sphactor_rate_tester, "SET TIMEOUT");
    zstr_send(sphactor_rate_tester, "16");
    zstr_send(sphactor_rate_tester, "TIMEOUT");

    zstr_free(&ret);
    ret = zstr_recv(sphactor_rate_tester);

    assert( streq( ret, "16") );
    zstr_free(&ret);
    zclock_sleep(1000/60);
    zactor_destroy (&sphactor_rate_tester);

    // lifecycle test
    sphactor_shim_t lifecycle_tester = { &sph_actor_lifecycle, NULL, NULL, "lifecycle_tester" };
    zactor_t *sphactor_lifecycle_tester = zactor_new (sphactor_actor_run, &lifecycle_tester);
    assert(sphactor_lifecycle_tester);
    zclock_sleep(200);
    zactor_destroy( &sphactor_lifecycle_tester );

    // reporting test
    sphactor_shim_t report_tester = { &sph_actor_reportertest, NULL, NULL, "reporter_tester" };
    zactor_t *sphactor_reportertest = zactor_new (sphactor_actor_run, &report_tester);
    assert(sphactor_reportertest);
    // acquire a pointer to the running sphactor_actor instance
    rc = zstr_send( sphactor_reportertest, "INSTANCE" );
    assert( rc == 0);
    sphactor_actor_t *repact;
    rc = zsock_recv (sphactor_reportertest, "p", &repact);
    assert( rc == 0 );
    assert(repact);
    // set the timeout to 1ms
    rc = zstr_sendm( sphactor_reportertest, "SET TIMEOUT" );
    rc = zstr_sendf( sphactor_reportertest, "%li", 1 );
    assert( rc == 0);
    // stress test the internal acquiring of the report
    for (int i=0; i<10;i++)
    {
        /*****
         * This will show a a status of 1 (idle) and 5 (timed event)
         * on my machine it tries about 30000 times before a new report
         * is acquired. So about 33 nanoseconds per request.
         */
        sphactor_report_t *r = sphactor_actor_atomic_report(repact);
        int count = 0;
        while ( !r)
        {
            count++;
            r = sphactor_actor_atomic_report(repact);
        }
        zsys_info("status: %i, iterations: %i, tried requests: %i", sphactor_report_status(r), sphactor_report_iterations(r), count );
        sphactor_report_destroy(&r);
    }
    zactor_destroy( &sphactor_reportertest );

    // zpoller add / remove test


    //  @end
    zsys_shutdown();  //  needed by windows
    printf ("OK\n");
}
