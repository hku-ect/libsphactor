/*  =========================================================================

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
    libsphactor implements an actor model using czmq zactor
    and pub/sub sockets for the actors to communicate with
    each other.

    A zactor needs a shim which contains the process to run and its parameters

@discuss
@end
*/

#include "sphactor_classes.h"

//  Structure of our class

struct _sphactor_t {
    zactor_t *actor;            //  A Sphactor instance wraps a zactor
    char    *name;              //  Copy of our actor's name
    zuuid_t *uuid;              //  Copy of our actor's uuid
    char    *endpoint;          //  Copy of our actor's endpoint
    char    *type;              //  Copy of our actor's type name
    zlist_t *subscriptions;     //  Copy of our actor's (incoming) connections
    zconfig_t *capability;      //  Capability of this actor
    zhash_t *values_cache;      //  Cached values from the capabilities
    float   posx;               //  XY position is used when visualising actors
    float   posy;
    sphactor_report_t *latest_report;   //  The latest report acquired from the actor
    sphactor_actor_t  *_sph_act;        //  pointer to the actor in the thread, internal use only!
};

//  Hash table for the actor_type register of actors
static zhash_t *actors_reg = NULL;
static zlist_t *actors_keys = NULL;

//  (forward declare)
void sphactor_actor_run(zsock_t *pipe, void *args);

//  --------------------------------------------------------------------------
//  Create a new sphactor. Pass a name and uuid. If your specify NULL
//  a uuid will be generated and the first 6 chars will be used as a name
sphactor_t *
sphactor_new (sphactor_handler_fn handler, void *args, const char *name, zuuid_t *uuid)
{
    sphactor_t *self = (sphactor_t *) zmalloc (sizeof (sphactor_t));
    assert (self);

    if (uuid)
        self->uuid = zuuid_dup(uuid);

    sphactor_shim_t shim = { handler, args, uuid, name };
    self->actor = zactor_new( sphactor_actor_run, &shim);
    self->latest_report = NULL;
    self->_sph_act = NULL;
    if (name)
    {
        //self->name = strdup(name);
        sphactor_ask_set_name( self, name );
    }
    self->type = NULL;
    self->endpoint = NULL;
    self->subscriptions = zlist_new();
    self->capability = NULL;
    self->values_cache = zhash_new();
    zhash_autofree(self->values_cache); // we're using strings for now
    self->posx = 0;
    self->posy = 0;
    zlist_comparefn (self->subscriptions, (zlist_compare_fn *) strcmp);
    zlist_autofree (self->subscriptions); // only works for char *
    return self;
}

sphactor_t *
sphactor_new_proc(const char *type, const char *name, zuuid_t *uuid)
{
    zproc_t *proc = zproc_new();
    assert(proc);
    //zproc_set_verbose(proc, true);

    zproc_set_argsx(proc, "./sph", "--actor", "Log", NULL);
    zproc_set_stdout (proc, NULL);
    zproc_set_stderr (proc, NULL);
    zproc_set_stdin (proc, NULL);

    int r = zproc_run (proc);
    zclock_sleep (100); // to let actor start the process
    assert (r != -1);
    zsys_info("pid is: %i", zproc_pid(proc));
    zclock_sleep(1000);

    zsys_info("here we go");
    zsock_t *ctrlsock = zsock_new( ZMQ_DEALER );
    assert(ctrlsock);
    int rc = zsock_bind(ctrlsock, "tcp://*:4321");
    assert(rc != -1);
    zsock_wait (ctrlsock);

    //zstr_sendm(ctrlsock, "MEPRC");
    zstr_send(ctrlsock, "UUID");
    zuuid_t *uid = zuuid_new();
    rc = zsock_recv( ctrlsock, "U", &uid );
    assert(rc != -1);
    zsys_info("proc uuid: %s", zuuid_str(uid) );

    zstr_send(ctrlsock, "ENDPOINT");
    char *e = zstr_recv( ctrlsock );
    zsys_info("proc endpoint: %s", e );
    zstr_free(&e);
    zclock_sleep (1000); // to let actor start the process

    if (zstr_send (ctrlsock, "$TERM") == 0)
        zsock_wait (ctrlsock);

    zsock_destroy(&ctrlsock);
    //zproc_kill(proc, 15);
    zproc_destroy(&proc);

    return NULL;
}

sphactor_t *
sphactor_new_by_type (const char *actor_type, const char *name, zuuid_t *uuid)
{
    assert(actors_reg); // make sure something has ever been registered
    sphactor_funcs_t *funcs = (sphactor_funcs_t *)zhash_lookup( actors_reg, actor_type);
    if ( funcs == NULL )
    {
        zsys_error("%s type does not exist as a registered actor type", actor_type);
        return NULL;
    }
    // run constructor if any
    void *instance = NULL;
    if ( funcs->constructor )
        instance = funcs->constructor(funcs->constructor_args);
    sphactor_t *self = sphactor_new( funcs->handler, instance, name, uuid);
    assert( self );
    if (funcs->capability)
    {
        zconfig_t *capability = zconfig_dup(funcs->capability);
        sphactor_set_capability(self, capability);
    }
    sphactor_ask_set_actor_type(self, actor_type);
    return self;
}

sphactor_t *
sphactor_load(const zconfig_t *config)
{
    assert( config );
    zconfig_t* uuid = zconfig_locate((zconfig_t *)config, "uuid");
    zconfig_t* type = zconfig_locate((zconfig_t *)config, "type");
    zconfig_t* name = zconfig_locate((zconfig_t *)config, "name");
    zconfig_t* endpoint = zconfig_locate((zconfig_t *)config, "endpoint");
    zconfig_t* xpos = zconfig_locate((zconfig_t *)config, "xpos");
    zconfig_t* ypos = zconfig_locate((zconfig_t *)config, "ypos");

    char *uuidStr = zconfig_value(uuid);
    char *typeStr = zconfig_value(type);
    char *nameStr = zconfig_value(name);
    char *endpointStr = zconfig_value(endpoint);
    char *xposStr = zconfig_value(xpos);
    char *yposStr = zconfig_value(ypos);

    zuuid_t *uid = zuuid_new();
    zuuid_set_str(uid, uuidStr);
    // create actor
    sphactor_t* new_actor = sphactor_new_by_type(typeStr, nameStr, uid);
    assert(new_actor);

    // set position
    sphactor_set_position( new_actor, atof(xposStr), atof(yposStr) );
    //  TODO: we don't handle the endpoint (yet)

    //  We're assuming the ypos is the last thing added by the sphactor serialization
    //  from there we ready until we receive null and send that to the high-level actor
    //sph_deserialise_actor_data(new_actor, config);
    zconfig_t *capability = sphactor_capability(new_actor);
    if ( capability )
    {
        zconfig_t *cnf = zconfig_next(ypos);
        while ( cnf != NULL )
        {
            char *cnfStr = zconfig_value(cnf);
            char *cnfNme = zconfig_name(cnf);

            zconfig_t *root = zconfig_locate(capability, "capabilities");
            //  here we have to lookup cnfNme in the capabilities to see if it matches
            //  in order to find the api_call key of the capability with its optional format
            if ( root ) {
                zconfig_t *data = zconfig_locate(root, "data");
                while ( data ) {
                    zconfig_t *name = zconfig_locate(data, "name");
                    if (name && streq(zconfig_value(name), cnfNme) )
                    {
                        // TODO: here's the value is set in the capability, this is not safe!!!
                        zconfig_t *value = zconfig_locate(data, "value");
                        if ( value )
                        {
                            zconfig_set_value(value, "%s", cnfStr);
                        }
                        zconfig_t *zapic = zconfig_locate(data, "api_call");
                        if (zapic)
                        {
                            zconfig_t *zapiv = zconfig_locate(data, "api_value");
                            if (zapiv)
                                sphactor_ask_api(new_actor, zconfig_value(zapic), zconfig_value(zapiv), cnfStr);
                            else
                                sphactor_ask_api(new_actor, zconfig_value(zapic), "", cnfStr);
                        }
                    }
                    data = zconfig_next(data);
                }
            }
            cnf = zconfig_next(cnf);
        }
    }
    else
    {
        zsys_warning("No capability for this actor, thus no automatic API calls!");
    }

    //free(uuidStr);
    //free(typeStr);
    //free(endpointStr);

    return new_actor;
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
        if (self->type)
            zstr_free(&self->type);
        if (self->capability)
            zconfig_destroy(&self->capability);
        zhash_destroy(&self->values_cache);
        // free the report cache
        if ( self->latest_report ) sphactor_report_destroy(&self->latest_report);
        self->latest_report = NULL;
        self->_sph_act = NULL;   //  we don't own the pointer!!
        zlist_destroy(&self->subscriptions);  // the list uses autofree!
        //  Free object itself
        free (self);
        *self_p = NULL;
    }
}


zconfig_t *
sphactor_capability(sphactor_t *self)
{
    assert(self);
    return self->capability;
}

int
sphactor_set_capability(sphactor_t *self, zconfig_t *capability)
{
    assert(self);
    assert(capability);
    if (self->capability)
    {
        zconfig_destroy(&capability);
        return -1;
    }
    self->capability = capability;
    zconfig_t *capitem = zconfig_locate(self->capability, "capabilities/data");
    int rc = -1;
    while (capitem != NULL)
    {
        zconfig_t* name = zconfig_locate(capitem, "name");
        zconfig_t* value = zconfig_locate(capitem, "value");
        zconfig_t *zapic = zconfig_locate(capitem, "api_call");

        if (zapic)
        {
            zconfig_t *zapiv = zconfig_locate(capitem, "api_value");
            if (zapiv)
                rc = sphactor_ask_api(self, zconfig_value(zapic), zconfig_value(zapiv), zconfig_value(value));
            else // no api value so send default
                rc = sphactor_ask_api(self, zconfig_value(zapic), "", zconfig_value(value));

        }
        else if (value)
            rc = sphactor_ask_api(self, zconfig_value(name), "", zconfig_value(value));

        capitem = zconfig_next(capitem);
    }
    return rc;
 }

// caller does not own the uuid!
zuuid_t *
sphactor_ask_uuid (sphactor_t *self)
{
    assert(self);
    if (self->uuid == NULL )
    {
        // I'm not sure if this is safe if there's already a queue
        // on the actor's pipe???
        zstr_send(self->actor, "UUID");
        //self->uuid = zuuid_new(); // or just malloc it?
        int rc = zsock_recv( self->actor, "U", &self->uuid );
        assert ( rc==0 );
        assert ( self->uuid );
    }
    return self->uuid;
}

const char *
sphactor_ask_name (sphactor_t *self)
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
sphactor_ask_actor_type (sphactor_t *self)
{
    assert(self);
    if ( self->type == NULL )
    {
        zstr_send(self->actor, "TYPE");
        self->type = zstr_recv( self->actor );
    }
    return self->type;
}

const char *
sphactor_ask_endpoint (sphactor_t *self)
{
    assert(self);
    if ( self->endpoint == NULL )
    {
        zstr_send(self->actor, "ENDPOINT");
        self->endpoint = zstr_recv( self->actor );
    }
    return self->endpoint;
}


void
sphactor_ask_set_name (sphactor_t *self, const char *name)
{
    assert (self);
    assert (name);
    zstr_sendx (self->actor, "SET NAME", name, NULL);
}

void
sphactor_ask_set_actor_type (sphactor_t *self, const char *actor_type)
{
    assert (self);
    assert (actor_type);
    if (self->type)
        zstr_free(&self->type);
    self->type = strdup(actor_type);  // cache immediatelly
    zstr_sendx (self->actor, "SET TYPE", actor_type, NULL);
}

//  Set the timeout of this Sphactor's actor. This is used for the timeout
//  of the poller so the sphactor actor is looped for a fixed interval. Note
//  that the sphactor's actor method receives a NULL message if it is
//  triggered by timeout event as opposed to when triggered by a socket
//  event. By default the timeout is -1 implying it never timeouts.
void
sphactor_ask_set_timeout (sphactor_t *self, int64_t timeout)
{
    assert (self);
    assert (timeout);
    zstr_sendm(self->actor, "SET TIMEOUT");
    zstr_sendf(self->actor, "%li", timeout);
}

//  Return the current timeout of this sphactor actor's poller. By default
//  the timeout is -1 which means it never times out but only triggers
//  on socket events.
int64_t
sphactor_ask_timeout (sphactor_t *self)
{
    assert (self);
    zstr_send(self->actor, "TIMEOUT");
    zmsg_t *response = zmsg_recv( self->actor );
    char *cmd = zmsg_popstr( response );
    assert( strlen( cmd ) );
    int64_t ret = atoll( cmd );
    zmsg_destroy( &response );
    zstr_free( &cmd );
    return ret;
}

int
sphactor_ask_connect (sphactor_t *self, const char *endpoint)
{
    assert(self);
    assert(endpoint);
    zstr_sendx( self->actor, "CONNECT", endpoint, NULL );
    zmsg_t *response = zmsg_recv( self->actor );
    char *cmd = zmsg_popstr( response );
    assert( streq( cmd, "CONNECTED"));
    char *dest = zmsg_popstr(response);
    assert(streq(dest, endpoint));
    char *rc = zmsg_popstr(response);
    assert ( streq(rc, "0") );
    int rci = streq(rc, "0") ? 0 : -1;
    
    // save our connection
    if ( zlist_exists(self->subscriptions, dest) )
    {
        zsys_warning("Connection %s already exists", dest);
        zstr_free(&dest);
    }
    else
    {
        zlist_append(self->subscriptions, dest); // list uses auto free so dest will be duped
    }

    zstr_free(&cmd);
    zstr_free(&dest);
    zstr_free(&rc);
    zmsg_destroy(&response);

    return rci;
}

int
sphactor_ask_disconnect (sphactor_t *self, const char *endpoint)
{
    assert(self);
    assert(endpoint);
    int rc = zstr_sendx( self->actor, "DISCONNECT", endpoint, NULL );
    assert( rc == 0);
    zmsg_t *response = zmsg_recv( self->actor );
    char *cmd = zmsg_popstr( response );
    assert( streq( cmd, "DISCONNECTED"));
    char *dest = zmsg_popstr(response);
    assert(streq(dest, endpoint));
    char *rcc = zmsg_popstr(response);
    assert ( streq(rcc, "0") );
    int rci = streq(rcc, "0") ? 0 : -1;

    // this does nothing if the endpoint is not in the list
    zlist_remove(self->subscriptions, dest);

    zstr_free(&cmd);
    zstr_free(&dest);
    zstr_free(&rcc);
    zmsg_destroy(&response);

    return rci;
}

zlist_t *
sphactor_connections(sphactor_t *self)
{
    assert(self);
    return self->subscriptions;
}

zsock_t *
sphactor_socket(sphactor_t *self)
{
    return (zsock_t*)zactor_resolve(self->actor);
}

void
sphactor_ask_set_verbose (sphactor_t *self, bool on)
{
    assert (self);
    zstr_sendm (self->actor, "SET VERBOSE");
    zstr_send( self->actor, on ? "TRUE" : "FALSE");
}

void
sphactor_ask_set_reporting (sphactor_t *self, bool on)
{
    assert (self);
    zstr_sendm (self->actor, "SET REPORTING");
    zstr_send( self->actor, on ? "TRUE" : "FALSE");
}

static int
sphactor_ask_api_native(sphactor_t *self, const char *api_format, ...)
{
    assert(self);
    assert(api_format);
    assert(strlen(api_format));
    va_list argptr;
    va_start(argptr, api_format);
    int rc = zsock_vsend( sphactor_socket(self), api_format, argptr );
    va_end(argptr);
    return rc;
}

int
sphactor_ask_api(sphactor_t *self, const char *api_call, const char *api_format, const char *value)
{
    assert(self);
    assert(strlen(api_call));
    assert(api_format);
    int rc = 0;
    // this is still somewhat narrow but works for now
    if ( strlen(api_format) == 1 )
    {
        char *fmt = (char *)malloc(3); //strlen(api_format) + 2); // +2 starting s and for the null-terminator
        strcpy(fmt, "s");
        strcat(fmt, api_format);
        char type = api_format[0];
        switch( type )
        {
            case 'b': {
                /*
                 * Do we use this? it's incorrect
                char *buf = new char[4];
                const char *zvalueStr = zconfig_value(zvalue);
                strcpy(buf, zvalueStr);
                SendAPI<char *>(zapi_call, zapiv, zvalue, &buf);
                zstr_free(&buf);
                */
                zsys_error("Unsupprted 'b' format char in sphactor_ask_api");
            } break;
            case 'i': {
                rc = zsock_send( sphactor_socket(self), fmt, api_call, atoi(value));
            } break;
            case 'f': {
                rc = zsock_send( sphactor_socket(self), fmt, api_call, atof(value));
            } break;
            case 's': {
                rc = zsock_send( sphactor_socket(self), fmt, api_call, value);
            } break;
            default: {
                zsys_error("Unsupported sphactor_ask_api call: api_call: %s, api_format: %s, value: %s", api_call, api_format, value);
                rc = -1;
            } break;
        }
        free(fmt);
    }
    else
        rc = zsock_send( sphactor_socket(self), "ss", api_call, value);

    if (rc == 0)
    {
        zhash_update(self->values_cache, api_call, (void *)value);
    }

    return rc;
}

void
sphactor_set_position (sphactor_t *self, float x, float y)
{
    assert (self);
    self->posx = x;
    self->posy = y;
}

float
sphactor_position_x (sphactor_t *self)
{
    assert (self);
    return self->posx;
}

float
sphactor_position_y (sphactor_t *self)
{
    assert (self);
    return self->posy;
}

zconfig_t *
sphactor_zconfig_new( const char* filename )
{
    zconfig_t *root = zconfig_new("root", NULL);
    
    //TODO: basic setup?
    return root;
}

zconfig_t *
sphactor_zconfig_append(sphactor_t *self, zconfig_t *root)
{
    zconfig_t *sphactors = zconfig_locate (root, "actors");
    if ( sphactors == NULL ) {
        sphactors = zconfig_new("actors", root);
    }
    
    zconfig_t *curActor = zconfig_new("actor", sphactors);
    
    zconfig_t *zuuid, *ztype, *zname, *zendpoint, *xpos, *ypos;
    zuuid = zconfig_new("uuid", curActor);
    ztype = zconfig_new("type", curActor);
    zname = zconfig_new("name", curActor);
    zendpoint = zconfig_new("endpoint", curActor);
    xpos = zconfig_new("xpos", curActor);
    ypos = zconfig_new("ypos", curActor);

    zconfig_set_value(zuuid, "%s", zuuid_str(sphactor_ask_uuid (self) ) );
    zconfig_set_value(ztype, "%s", sphactor_ask_actor_type(self) );
    zconfig_set_value(zname, "%s", sphactor_ask_name(self) );
    zconfig_set_value(zendpoint, "%s", sphactor_ask_endpoint(self) );
    zconfig_set_value(xpos, "%f", (float)sphactor_position_x(self) );
    zconfig_set_value(ypos, "%f", (float)sphactor_position_y(self) );

    // Parse current state of data capabilities
    zconfig_t *cap = sphactor_capability(self);
    if ( cap ) {
        zconfig_t *root = zconfig_locate(cap, "capabilities");
        if ( root ) {
            zconfig_t *data = zconfig_locate(root, "data");
            while ( data ) {
                zconfig_t *name = zconfig_locate(data,"name");
                zconfig_t *value = zconfig_locate(data,"value");

                if (value) // only store if there's a value
                {
                    char *nameStr = zconfig_value(name);
                    char *valueStr = zconfig_value(value);

                    zconfig_t *stored = zconfig_new(nameStr, curActor);
                    zconfig_set_value(stored, "%s", valueStr);
                }

                data = zconfig_next(data);
            }
        }
    }

    return curActor;
}

// create a zconfig for this actor and optionally set a parent config
zconfig_t *
sphactor_save(sphactor_t *self, zconfig_t *parent)
{
    zconfig_t *curActor = zconfig_new("actor", parent);

    zconfig_t *zuuid, *ztype, *zname, *zendpoint, *xpos, *ypos;
    zuuid = zconfig_new("uuid", curActor);
    ztype = zconfig_new("type", curActor);
    zname = zconfig_new("name", curActor);
    zendpoint = zconfig_new("endpoint", curActor);
    xpos = zconfig_new("xpos", curActor);
    ypos = zconfig_new("ypos", curActor);

    zconfig_set_value(zuuid, "%s", zuuid_str(sphactor_ask_uuid (self) ) );
    zconfig_set_value(ztype, "%s", sphactor_ask_actor_type(self) );
    zconfig_set_value(zname, "%s", sphactor_ask_name(self) );
    zconfig_set_value(zendpoint, "%s", sphactor_ask_endpoint(self) );
    zconfig_set_value(xpos, "%f", (float)sphactor_position_x(self) );
    zconfig_set_value(ypos, "%f", (float)sphactor_position_y(self) );

    // get all capability data
    if (self->capability)
    {
        zconfig_t *root = zconfig_locate(self->capability, "capabilities");
        if ( root ) {
            zconfig_t *data = zconfig_locate(root, "data");
            while ( data ) {
                zconfig_t *name = zconfig_locate(data,"name");
                zconfig_t *apic = zconfig_locate(data,"api_call");
                char *nameStr = zconfig_value(name);
                char *valueStr;
                if (apic)
                {
                    char *apiStr = zconfig_value(apic);
                    valueStr = (char *)zhash_lookup(self->values_cache, apiStr);
                }
                else
                    valueStr = (char *)zhash_lookup(self->values_cache, nameStr);

                if (valueStr) // only store if there's a value
                {
                    zconfig_t *stored = zconfig_new(nameStr, curActor);
                    zconfig_set_value(stored, "%s", valueStr);
                }
                data = zconfig_next(data);
            }
        }
    }
    return curActor;
}

sphactor_report_t *
sphactor_report(sphactor_t *self)
{
    if ( self->_sph_act == NULL )
    {
        int rc = zstr_send( self->actor, "INSTANCE" );
        assert( rc == 0);

        rc = zsock_recv (self->actor, "p", &self->_sph_act);
        assert( rc == 0 );
        if (  self->_sph_act == NULL )
        {
            zsys_error( "error requesting the instance pointer for the report" );
            return NULL;
        }
    }
    // swap the report pointer atomically with NULL
    sphactor_report_t *report = sphactor_actor_atomic_report( self->_sph_act );
    // if we receive a NULL report this means there's no update in the status
    // the latest report is then still valid.
    if ( report == NULL ) return self->latest_report;
    // we now save the report as the latest report and destroy the old one
    // if the old one is not NULL
    if ( self->latest_report ) sphactor_report_destroy(&self->latest_report);
    self->latest_report = report;
    // we now own report so the caller must destroy when finished with it
    // unless it's null of course
    return self->latest_report;
}

int
sphactor_register(const char *actor_type, sphactor_handler_fn handler, zconfig_t *capability, sphactor_constructor_fn constructor, void *constructor_args)
{
    if (actors_reg == NULL ) actors_reg = zhash_new();  // initializer
    char *item = (char*)zhash_lookup(actors_reg, actor_type);
    if ( item != NULL )
    {
        zsys_error("%s is already registered", actor_type);
        return -1;
    }

    sphactor_funcs_t *funcs = (sphactor_funcs_t *) zmalloc (sizeof (sphactor_funcs_t));
    assert (funcs);
    funcs->handler = handler;
    funcs->capability = capability; // can be NULL;
    funcs->constructor = constructor; // can be NULL
    funcs->constructor_args = constructor_args; // can be NULL

    int rc = zhash_insert(actors_reg, actor_type, (void*)funcs);
    assert( rc == 0 );
    return rc;
}

//  Unregister a actor_type specified by key from the actors_reg hash table.
//  If there was no such item, this function does nothing.
int
sphactor_unregister( const char *actor_type)
{
    sphactor_funcs_t *item = (sphactor_funcs_t*)zhash_lookup(actors_reg, actor_type);
    if ( item == NULL )
    {
        zsys_error("no %s type is found", actor_type);
        return -1;
    }
    //  this will not touch running actors!!!
    sphactor_funcs_t *funcs = (sphactor_funcs_t *)zhash_lookup( actors_reg, actor_type );
    if (funcs->capability)
        zconfig_destroy(&funcs->capability);
    zhash_delete( actors_reg, actor_type );
    // update actors_keys
    zlist_destroy(&actors_keys);
    actors_keys = zhash_keys(actors_reg);
    free(item);
    item = NULL;
    return 0;
}

//
//  Returns keys list of registered actor types (can be empty if no actors were registered)
//   Implementations are expected to register themselves prior to requesting this list.
zhash_t *
sphactor_get_registered ()
{
    return actors_reg;
}

//
//  Disposes of all statically allocated resources
//   Call this before shutting down, because otherwise these resources will be re-allocated as needed.
void
sphactor_dispose ()
{
    zlist_destroy(&actors_keys);
    // cleanup the registered actors
    void *it = zhash_first(actors_reg);
    while (it)
    {
        sphactor_funcs_t *f = (sphactor_funcs_t *)it;
        if( f->capability)
            zconfig_destroy(&f->capability);
        free(it);
        it = zhash_next(actors_reg);
    }
    zhash_destroy(&actors_reg);
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

static zmsg_t *
hello_sphactor(sphactor_event_t *ev, void *args)
{
    if ( ev->msg == NULL ) return NULL;
    assert(ev->msg);
    //  just echo what we receive
    char *cmd = zmsg_popstr(ev->msg);
    zsys_info("Hello actor %s says: %s", ev->name, cmd);
    zstr_free(&cmd);
    // if there are strings left publish them
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

static char *capability_string =
                          "capabilities\n"
                          "    data\n"
                          "        name = \"rate\"\n"
                          "        type = \"int\"\n"
                          "        value = \"60\"\n"
                          "        min = \"1\"\n"
                          "        max = \"1000\"\n"
                          "        step = \"1\"\n"
                          "    data\n"
                          "        name = \"someFloat\"\n"
                          "        type = \"float\"\n"
                          "        value = \"1.0\"\n"
                          "        min = \"0\"\n"
                          "        max = \"10\"\n"
                          "        step = \"0\"\n"
                          "    data\n"
                          "        name = \"someText\"\n"
                          "        type = \"string\"\n"
                          "        value = \"Hello world!\"\n"
                          "        max = \"64\"\n";

static zmsg_t *
hello_sphactor2(sphactor_event_t *ev, void *args)
{
    if ( streq( ev->type, "INIT" ) )
    {

        int rc = sphactor_actor_set_capability((sphactor_actor_t *)ev->actor, zconfig_str_load(capability_string));
        assert(rc == 0);
    }
    if ( streq( ev->type, "API") )
    {
        char *cmd = zmsg_popstr(ev->msg);
        if ( streq(cmd, "someFloat") )
        {
            zframe_t *f = zmsg_pop(ev->msg);
            float *flt = (float *)zframe_data(f);
            assert( *flt - 1.0f < FLT_EPSILON);
            zframe_destroy(&f);
        }
        else if ( streq(cmd, "someText") )
        {
            char *s = zmsg_popstr(ev->msg);
            assert( streq(s, "Hello world!") );
            zstr_free(&s);
        }

        zstr_free(&cmd);
    }
    if ( ev->msg == NULL ) return NULL;
    assert(ev->msg);
    // just echo what we receive
    char *cmd = zmsg_popstr(ev->msg);
    zsys_info("Hello2 actor %s says: %s", ev->name, cmd);
    zstr_free(&cmd);
    // if there are strings left publish them
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
api_sphactor(sphactor_event_t *ev, void *args)
{
    if ( ev->msg == NULL ) return NULL;
    assert(ev->msg);
    //  just echo what we receive
    char *cmd = zmsg_popstr(ev->msg);
    zsys_info("Api actor %s says: %s", ev->name, cmd);
    assert( streq(cmd, "TESTAPI") );
    while( zmsg_size(ev->msg) )
    {
        char *tmp = zmsg_popstr(ev->msg);
        zsys_info("Api actor %s also says: %s", ev->name, tmp);
        assert(streq(tmp, "123"));
        zstr_free(&tmp);
    }
    zstr_free(&cmd);
    zmsg_destroy(&ev->msg);
    return NULL;
}

typedef struct {
    char * name;
} regtest_actor;

static void *
regtest_constructor(void *args) {
    regtest_actor *inst = (regtest_actor *) zmalloc (sizeof (regtest_actor));
    assert(inst);
    zsys_info("regtest_constructor: ");
    inst->name = strdup((char *)args);  // we're dupping because windows would otherwise fail?
    return inst;
}

static zmsg_t *
regtest_handler( sphactor_event_t *ev, void* args)
{
    zsys_info("test_handler: typ: %s", ev->type);
    regtest_actor *inst = (regtest_actor *)args;
    assert( streq(inst->name, "test") );
    zsys_info("regtest_actor name: %s", inst->name);
    if ( streq( ev->type, "DESTROY" ) )
    {
        zstr_free( &inst->name );
        free( inst );
        inst = NULL;
    }
    return NULL;
}

static zmsg_t *
hello_report(sphactor_event_t *ev, void *args)
{
    if ( ev->msg == NULL ) return NULL;
    assert(ev->msg);
    
    // check if this is a report request, or a
    //  just echo what we receive
    char *cmd = zmsg_popstr(ev->msg);
    zsys_info("Received from %s message: %s", ev->name, cmd);
    
    if ( streq(cmd, "CLEAR")) {
        // clear our internal message
        sphactor_actor_set_custom_report_data((sphactor_actor_t *)ev->actor, NULL);
    }
    else {
        // Generate custom osc message for report
        char* msgBuffer = (char*) zmalloc(64);
        memcpy (msgBuffer, cmd, strlen(cmd));
        zosc_t * oscMsg = zosc_create("/message", "s", msgBuffer);
        assert(oscMsg);
        zstr_free(&msgBuffer);
        
        //TODO: Need to be able to set msg on actor instance
        sphactor_actor_set_custom_report_data((sphactor_actor_t *)ev->actor, oscMsg);
    }
    
    zstr_free(&cmd);
    
    // if there are strings left publish them
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
spawn_sphactor(sphactor_event_t *ev, void *args)
{
    if ( ev->msg == NULL ) return NULL;
    assert(ev->msg);
    //  just echo what we receive
    char *msg = zmsg_popstr(ev->msg);
    zsys_info("Hello actor %s says: %s", ev->name, msg);
    zstr_free(&msg);
    // if there are strings left publish them
    zmsg_addstrf( ev->msg, "HELLO from %s", ev->name);
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

void
sphactor_test (bool verbose)
{
    printf (" * sphactor: ");
    // register unregister test
    actors_reg = zhash_new();
    sphactor_register("hello", &hello_sphactor, NULL, NULL, NULL);
    sphactor_funcs_t *item = (sphactor_funcs_t*)zhash_lookup(actors_reg, "hello");
    assert(item);
    assert( item->handler == &hello_sphactor );
    sphactor_unregister("hello");
    item = (sphactor_funcs_t*)zhash_lookup(actors_reg, "hello");
    assert( item == NULL );
    assert( zhash_size(actors_reg) == 0 );
    // register and construction test
    sphactor_register("test", regtest_handler, NULL, regtest_constructor, "test");
    item = (sphactor_funcs_t*)zhash_lookup(actors_reg, "test");
    assert(item);
    assert(item->handler);
    assert(item->constructor);
    assert(item->handler == regtest_handler );
    assert(item->constructor == regtest_constructor );
    // run constructor
    regtest_actor *instance = (regtest_actor *)item->constructor(item->constructor_args);
    assert( streq(instance->name, "test" ) );
    // start actor
    sphactor_t *regtestactor = sphactor_new(item->handler, (void *)instance, NULL, NULL);
    // actor will display event msgs
    zclock_sleep(100);
    sphactor_unregister("test");
    sphactor_destroy(&regtestactor);
    zhash_destroy(&actors_reg);

    //  @selftest
    //  Simple create/destroy/name/position/uuid test
    sphactor_t *self = sphactor_new ( hello_sphactor, NULL, NULL, NULL);
    assert (self);
    const zuuid_t *uuidtest = sphactor_ask_uuid(self);
    assert(uuidtest);
    // position test
    sphactor_set_position(self, 23.f, 24.f);
    assert( sphactor_position_x(self) == 23.f );
    assert( sphactor_position_y(self) == 24.f );
    //  name should be the first 6 chars from the uuid
    const char *name = sphactor_ask_name( self );
    char *name2 = (char *) zmalloc (7);
    memcpy (name2, zuuid_str((zuuid_t *)uuidtest), 6);
    assert( streq ( name, name2 ));
    zstr_free(&name2);
    //zuuid_destroy(&uuidtest); //sphactor_ask_uuid is owner of the pointer!
    sphactor_destroy (&self);

    //  Simple create/destroy/name/uuid test with specified uuid
    zuuid_t *uuid = zuuid_new();
    self = sphactor_new ( NULL, NULL, NULL, uuid);
    assert (self);
    uuidtest = sphactor_ask_uuid(self);
    assert(uuidtest);
    assert( zuuid_eq(uuid, zuuid_data((zuuid_t*)uuidtest) ) );
    //  name should be the first 6 chars from the uuid
    name = sphactor_ask_name( self );
    name2 = (char *) zmalloc (7);
    memcpy (name2, zuuid_str(uuid), 6);
    assert( streq ( name, name2 ));
    zstr_free(&name2);
    //  test timeout setting and getting
    sphactor_ask_set_timeout(self, 1000);
    assert( sphactor_ask_timeout( self ) == 1000);
    sphactor_destroy (&self);

    //  Simple create/destroy/connect/disconnect test
    sphactor_t *pub = sphactor_new ( hello_sphactor, NULL, NULL, NULL);
    sphactor_t *sub = sphactor_new ( hello_sphactor, NULL, NULL, NULL);
    assert (pub);
    assert (sub);
    sphactor_ask_set_verbose(sub, true);
    //  get endpoints
    const char *pubendp = sphactor_ask_endpoint(pub);
    const char *subendp = sphactor_ask_endpoint(sub);
    zuuid_t *puuid = sphactor_ask_uuid(pub);
    zuuid_t *suuid = sphactor_ask_uuid(sub);
    char *endpointest = (char *)malloc( (10 + strlen(zuuid_str(puuid) ) )  * sizeof(char) );
    sprintf( endpointest, "inproc://%s", zuuid_str(puuid));
    assert( streq( pubendp, endpointest));
    sprintf( endpointest, "inproc://%s", zuuid_str(suuid));
    assert( streq( subendp, endpointest));
    //  connect sub to pub
    int rc = sphactor_ask_connect(sub, pubendp);
    assert( rc == 0);
    assert( zlist_exists(sphactor_connections(sub), (void *)pubendp) );

    //  disconnect sub to pub
    rc = sphactor_ask_disconnect(sub, pubendp);
    assert( rc == 0);
    assert( ! zlist_exists(sphactor_connections(sub), (void *)pubendp) );

    zstr_free( &endpointest );
    sphactor_destroy (&pub);
    sphactor_destroy (&sub);


    // sphactor_hello test
    sphactor_t *hello1 = sphactor_new ( hello_sphactor, NULL, NULL, NULL);
    sphactor_t *hello2 = sphactor_new ( hello_sphactor2, NULL, NULL, NULL);
    sphactor_ask_connect(hello1, sphactor_ask_endpoint(hello2));
    sphactor_ask_connect(hello2, sphactor_ask_endpoint(hello1));
    zstr_sendm(hello1->actor, "SEND");
    zstr_sendm(hello1->actor, "HELLO");
    zstr_sendm(hello1->actor, "WORLD");
    zstr_sendm(hello1->actor, "AND");
    zstr_sendm(hello1->actor, "ALIEN");
    zstr_send(hello1->actor, "SPACELINGS");
    zclock_sleep(10); //  give some time for the test to complete, since it's threaded
    sphactor_destroy (&hello1);
    sphactor_destroy (&hello2);

    /* *** mega spawn test
     *
     *  The maximum number of sphactors is determined by a number of factors:
     *  (a) first we have the OS open files limit (see ulimit -n on *nix) usually 1024
     *  (b) zactor uses two file descriptors per instance for signaling and communications
     *  (c) sphactor uses two file desciptors for the pub and sub socket per instance.
     *  (d) every process on *nix gets 3 file descriptors for stdout/-in/-err
     *
     *  Theoretically this gives us the following equation for the maximum number of (sph)actors
     *  n = (a/(b+c))-d i.e. (1024/(2+2))-3 = 253
     *  In practice the number is 252
     *
     */
    int64_t start = zclock_usecs();
    sphactor_t *prev = NULL;
    zlist_t *spawned_actors = zlist_new();
    
    //rc = putenv("SPHACTOR_SOCKET_LIMIT=250");
    //assert(rc==0);
    
    //TODO: Set this variable from an environment variable
    int limit = 250;
    char *limitStr = getenv("SPHACTOR_SOCKET_LIMIT");
    if ( limitStr != NULL ) {
        limit = atoi(limitStr);
    }
    
    // we'll start sockLim actors which will be formed into a single chain.
    // each actor will send hello to the connected actor, wich is started
    // from the last created actor
    for (int i=0; i<limit; i++)
    {
        sphactor_t *spawn = sphactor_new( spawn_sphactor, NULL, NULL, NULL );
        if (verbose)
            zsys_info("Sphactor number %d %s spawned", i+1, sphactor_ask_name(spawn));
        else {
            sphactor_ask_name(spawn);   // to cache the name (somehow NAME ends up in actor) Hello actor DB0A21 says: NAME
        }
        if (prev)
            sphactor_ask_connect(prev, sphactor_ask_endpoint(spawn));
        zlist_append(spawned_actors, spawn);
        prev = spawn;
        zclock_sleep(10);
    }
    int64_t end = zclock_usecs();
    zsys_info("%i Actors spawned in %d microseconds (%.6f ms)", limit, end-start, (end-start)/1000.f);
    zclock_sleep(2000);
    zstr_sendm(prev->actor, "SEND");
    zstr_sendf(prev->actor, "HELLO from %s", sphactor_ask_name(prev));
    zclock_sleep(200);
    while (zlist_size(spawned_actors) > 0)
    {
        sphactor_t *act = (sphactor_t *)zlist_pop(spawned_actors);
        sphactor_destroy( &act );
    }
    assert(zlist_size(spawned_actors) == 0);
    zlist_destroy(&spawned_actors);
    zsys_info("destroyed spawned actors");
    
    // actor serialization test
    // create two actors
    sphactor_t *actor1 = sphactor_new(NULL, NULL, "Actor 1", NULL);
    sphactor_t *actor2 = sphactor_new(NULL, NULL, "Actor 2", NULL);
    sphactor_ask_set_actor_type(actor1, "Type1");
    sphactor_ask_set_actor_type(actor2, "Type2");
    
    // save to zconfig file
    const char* fileName = "testsave.txt";
    zconfig_t * config = sphactor_zconfig_new(fileName);
    sphactor_zconfig_append(actor1, config);
    sphactor_zconfig_append(actor2, config);
    zconfig_save(config, fileName);
    
    zconfig_destroy(&config);
    
    // load zconfig file, find actors
    config = zconfig_load(fileName);
    zconfig_t *sphactors = zconfig_locate(config, "actors");
    zconfig_t *sphact1 = zconfig_locate(sphactors, "actor");
    zconfig_t *sphact2 = zconfig_next(sphact1);
    
    char* sphact1uuid = zconfig_value(zconfig_locate(sphact1, "uuid"));
    char* sphact2uuid = zconfig_value(zconfig_locate(sphact2, "uuid"));
    
    // check that the saved uuid's are correct
    zsys_info("%s == %s", zuuid_str(actor1->uuid), sphact1uuid);
    zsys_info("%s == %s", zuuid_str(actor2->uuid), sphact2uuid);
    
    assert(streq(zuuid_str(actor1->uuid),sphact1uuid));
    assert(streq(zuuid_str(actor2->uuid),sphact2uuid));
    
    remove(fileName);
    zconfig_destroy(&config);
    sphactor_destroy(&actor1);
    sphactor_destroy(&actor2);
    ///
    /// Report test
    ///
    {
        // Create two actors from the same function
        sphactor_t *sender = sphactor_new ( hello_sphactor, NULL, NULL, NULL);
        sphactor_t *reporter = sphactor_new ( hello_report, NULL, NULL, NULL);
        assert(sender);
        assert(reporter);
        
        // Connect them
        sphactor_ask_connect(sender, sphactor_ask_endpoint(reporter));
        sphactor_ask_connect(reporter, sphactor_ask_endpoint(sender));
        
        // Have them send a message to each other
        const char* msg12 = "1 talking to 2";
        zstr_sendm(sender->actor, "SEND");
        zstr_send(sender->actor, msg12);

        zclock_sleep(100);

        // get sender report
        sphactor_report_t *sender_rep = sphactor_report(sender);
        assert(sphactor_report_send_time(sender_rep) > 0);

        // Wait for the actor to be idle
        zsys_info("Requesting Report");
        sphactor_report_t *report = sphactor_report(reporter);
        assert(report);
        
        // read report status & message
        int reportrc = sphactor_report_status(report);
        assert(reportrc >= 0);
        assert(sphactor_report_recv_time(report) > 0);
        zosc_t* oscMsg = sphactor_report_custom(report);
        assert(oscMsg);
        
        char* txt;
        int rc = zosc_retr(oscMsg, "s", &txt);
        if ( rc == 0 ) {
            assert(streq(txt, msg12));
            zsys_info("Received report message matches sent message (%s)", txt);
            zstr_free(&txt);
        }
        
        // Send a message to clear the report
        zstr_sendm(sender->actor, "SEND");
        zstr_send(sender->actor, "CLEAR");

        zclock_sleep(100);
        
        // read report, and confirm message is NULL
        sphactor_report_t *report2 = sphactor_report(reporter);
        assert(report2);
        zosc_t* oscMsg2 = sphactor_report_custom(report2);
        assert(oscMsg2 == NULL);

        zsys_info("Confirmed received message was null");
        
        // Clean Up
        zclock_sleep(10); //  give some time for the test to complete, since it's threaded

        sphactor_destroy (&sender);
        sphactor_destroy (&reporter);
    }
    ///
    /// END
    ///

    // sphactor_report test
    sphactor_t *reportact = sphactor_new ( hello_sphactor, NULL, NULL, NULL);
    assert(reportact);
    sphactor_report_t *report = sphactor_report(reportact);
    assert( report );
    zsys_info("status = %i, should be %i or %i", sphactor_report_status( report ), SPHACTOR_REPORT_IDLE, SPHACTOR_REPORT_API );
    //  the status is either IDLE or API since to retrieve the report a first time
    //  the internal INSTANCE API command is used so in theory this can still
    //  be the status when we retrieve the report!
    assert( sphactor_report_status( report ) == SPHACTOR_REPORT_IDLE || sphactor_report_status( report ) == SPHACTOR_REPORT_API);
    assert( sphactor_report_send_time( report ) == 0);
    assert( sphactor_report_recv_time( report ) == 0);

    zclock_sleep(10);
    
    report = sphactor_report(reportact);
    assert( report );
    
    zclock_sleep(10);
    sphactor_destroy(&reportact);

    // sphactor capability test
    sphactor_t *capact = sphactor_new ( hello_sphactor, NULL, NULL, NULL);
    assert(capact);
    zconfig_t *cap = sphactor_capability(capact);
    // by default the capability is a null pointer
    assert(cap == NULL);
    sphactor_destroy(&capact);

    // sphactor capability test
    sphactor_t *capact2 = sphactor_new ( hello_sphactor2, NULL, NULL, NULL);
    assert(capact2);
    rc = sphactor_set_capability(capact2, zconfig_str_load(capability_string));
    assert(rc == 0);
    zconfig_t *cap2 = sphactor_capability(capact2);
    // by default the capability is a null pointer
    assert(cap2);
    // set capability again, should fail
    rc = sphactor_set_capability(capact2, zconfig_str_load(capability_string));
    assert( rc == -1);
    //  get actor config
    zconfig_t *actcnf = sphactor_save(capact2, NULL);
    assert(actcnf);
    zconfig_t *val = zconfig_locate(actcnf, "rate");
    assert( streq(zconfig_value(val), "60"));
    val = zconfig_locate(actcnf, "someFloat");
    assert( streq(zconfig_value(val), "1.0"));
    val = zconfig_locate(actcnf, "someText");
    assert( streq(zconfig_value(val), "Hello world!"));
    //  api calls based on capability
    zconfig_t *apiitem = zconfig_locate(cap2, "capabilities/data");
    while (apiitem != NULL)
    {
        zconfig_t* name = zconfig_locate(apiitem, "name");
        zconfig_t* value = zconfig_locate(apiitem, "value");
        zconfig_t *zapic = zconfig_locate(apiitem, "api_call");

        if (zapic)
        {
            zconfig_t *zapiv = zconfig_locate(apiitem, "api_value");
            if (zapiv)
                sphactor_ask_api(capact2, zconfig_value(zapic), zconfig_value(zapiv), zconfig_value(value));
            else // no api value so send default
                sphactor_ask_api(capact2, zconfig_value(zapic), "", zconfig_value(value));

        }
        else
            sphactor_ask_api(capact2, zconfig_value(name), "", zconfig_value(value));

        apiitem = zconfig_next(apiitem);
    }

    sphactor_destroy(&capact2);
    zconfig_destroy(&actcnf);

    // sphactor custom api test
    sphactor_t *apiact = sphactor_new ( api_sphactor, NULL, NULL, NULL);
    assert(apiact);
    // trigger custom api
    sphactor_ask_api(apiact, "TESTAPI", "i", "123");
    //zsock_send( sphactor_socket(apiact), "si", "TESTAPI", 123 );
    zclock_sleep(10);
    sphactor_destroy(&apiact);

    // sphactor_load test
    zconfig_t *root = zconfig_str_load (
    "actors\n"
    "    actor\n"
    "        uuid = \"7B21D87CB6B04FC5801A5B396269876D\"\n"
    "        type = \"Log\"\n"
    "        name = \"8FADA7\"\n"
    "        endpoint = \"inproc://8FADA7E8835E42D5AF85FC75F4A9B70E\"\n"
    "        xpos = \"502.500000\"\n"
    "        ypos = \"312.000000\"\n"
    /*"    actor\n"
    "        uuid = \"2A7110DFC47C4DF19EB1D17E390CF86B\"\n"
    "        type = \"Pulse\"\n"
    "        name = \"2A7110\"\n"
    "        endpoint = \"inproc://2A7110DFC47C4DF19EB1D17E390CF86B\"\n"
    "        xpos = \"34.500000\"\n"
    "        ypos = \"426.000000\"\n"
    "        timeout = \"1000\"\n"
    "        someFloat = \"1.0\"\n"
    "        someText = \"Hello world!\"\n"
    "connections\n"
    "    con = \"inproc://2A7110DFC47C4DF19EB1D17E390CF86B,inproc://7B21D87CB6B04FC5801A5B396269876D,OSC\"\n"
                */
    ///"    con = \"inproc://7B21D87CB6B04FC5801A5B396269876D,inproc://8FADA7E8835E42D5AF85FC75F4A9B70E,OSC\"\n"
    );

    // register Log
    rc = sphactor_register("Log", regtest_handler, NULL, regtest_constructor, "test");
    assert(rc == 0);
    sphactor_t *loadedactor = sphactor_load(zconfig_locate(root, "actors/actor"));
    assert(loadedactor);
    assert( streq(sphactor_ask_name(loadedactor), "8FADA7") );
    assert( streq(sphactor_ask_actor_type(loadedactor), "Log") );
    assert( streq(zuuid_str(sphactor_ask_uuid(loadedactor)), "7B21D87CB6B04FC5801A5B396269876D" ) );
    assert( fabs(sphactor_position_x(loadedactor)-502.5) < FLT_EPSILON );
    assert( fabs(sphactor_position_y(loadedactor)-312.0) < FLT_EPSILON );
    sphactor_destroy(&loadedactor);
    sphactor_unregister("Log");
    zconfig_destroy(&root);

    // sphactor_proc test
    //sphactor_t *proca = sphactor_new_proc("Log", NULL, NULL);
    //assert(proca == NULL);

    zsys_shutdown();  //  needed by Windows: https://github.com/zeromq/czmq/issues/1751
    //  @end
    printf ("OK\n");
}
