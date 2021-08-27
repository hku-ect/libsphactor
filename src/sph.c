/*  =========================================================================
    sph - description

    Copyright (c) 2020 the Contributors as noted in the AUTHORS file.

    This file is part of Sphactor, an open-source framework for high level
    actor model concurrency --- http://sphactor.org

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
    =========================================================================
*/

/*
@header
    sph -
@discuss
@end
*/

#include "sphactor_classes.h"
#include "sph_stock.h"
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <gnu/lib-names.h>

static int
print_help()
{
    puts ("sph [options] <config file>");
    puts ("  --verbose / -v         verbose output");
    puts ("  --help / -h            this information");
    puts ("  <config file>          stage config file to load");
    return 0;
}

int
send_api( sphactor_t *new_actor, char *apic, char *apifmt, char *value)
{
    assert(strlen(apic));
    assert(apifmt);
    assert(strlen(value));
    if ( strlen(apifmt) == 1 )
    {
        char *fmt = malloc(3); //strlen(apifmt) + 2); // +2 starting s and for the null-terminator
        strcpy(fmt, "s");
        strcat(fmt, apifmt);
        char type = apifmt[0];
        int rc = 0;
        switch( type )
        {
            case 'b': {
                /*
                 * Do we use this? it's incorrect
                char *buf = new char[4];
                const char *zvalueStr = zconfig_value(zvalue);
                strcpy(buf, zvalueStr);
                SendAPI<char *>(zapic, zapiv, zvalue, &buf);
                zstr_free(&buf);
                */
                zsys_error("Unsupprted 'b' format char in send_api");
            } break;
            case 'i': {
                rc = zsock_send( sphactor_socket(new_actor), fmt, apic, atoi(value));
            } break;
            case 'f': {
                rc = zsock_send( sphactor_socket(new_actor), fmt, apic, atof(value));
            } break;
            case 's': {
                rc = zsock_send( sphactor_socket(new_actor), fmt, apic, value);
            } break;
            default: {
                zsys_error("Unsupported send_api call: apic: %s, apifmt: %s, value: %s", apic, apifmt, value);
                rc = -1;
            } break;
        }
        free(fmt);
        return rc;
    }
    return zsock_send( sphactor_socket(new_actor), "si", apic, atoi(value));
}

// Loads an actor from the zconfig, returns NULL if failed.
sphactor_t *
sph_ctrl_actor_load(const zconfig_t *actor_conf)
{
    assert( actor_conf );
    zconfig_t* uuid = zconfig_locate(actor_conf, "uuid");
    zconfig_t* type = zconfig_locate(actor_conf, "type");
    zconfig_t* name = zconfig_locate(actor_conf, "name");
    zconfig_t* endpoint = zconfig_locate(actor_conf, "endpoint");
    zconfig_t* xpos = zconfig_locate(actor_conf, "xpos");
    zconfig_t* ypos = zconfig_locate(actor_conf, "ypos");

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
    sphactor_set_position( new_actor, atoi(xposStr), atoi(yposStr) );
    //  TODO: we don't handle the endpoint (yet)

    //  We're assuming the ypos is the last thing added by the sphactor serialization
    //  from there we ready until we receive null and send that to the high-level actor
    //sph_deserialise_actor_data(new_actor, actor_conf);
    zconfig_t *capability = sphactor_ask_capability(new_actor);
    assert(capability);
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
                    // match name the correct name
                    /*zconfig_t *value = zconfig_locate(data, "value");
                    if ( value )
                    {
                        zconfig_set_value(value, "%s", cnfStr);
                    }*/
                    zconfig_t *zapic = zconfig_locate(data, "api_call");
                    if (zapic)
                    {
                        zconfig_t *zapiv = zconfig_locate(data, "api_value");
                        if (zapiv)
                            send_api(new_actor, zconfig_value(zapic), zconfig_value(zapiv), cnfStr);
                        else
                            send_api(new_actor, zconfig_value(zapic), "", cnfStr);
                    }
                }
                data = zconfig_next(data);
            }
        }
        cnf = zconfig_next(cnf);
    }

    //free(uuidStr);
    //free(typeStr);
    //free(endpointStr);

    return new_actor;
}

static zhash_t* actor_list = NULL;

bool
sph_stage_load(const char *filepath)
{
    zconfig_t* root = zconfig_load(filepath);
    if ( root == NULL )
    {
        zsys_error("Error loading %s", filepath);
        return false;
    }

    if (actor_list == NULL)
        actor_list = zhash_new();

    zconfig_t* actors_conf = zconfig_locate(root, "actors");
    zconfig_t* actor_conf = zconfig_locate(actors_conf, "actor");
    // create actors
    while( actor_conf != NULL )
    {
        sphactor_t *new_actor =  sph_ctrl_actor_load(actor_conf);
        assert(new_actor);

        // save actor
        int rc = zhash_insert(actor_list, sphactor_ask_uuid(new_actor), new_actor);
        assert( rc == 0);

        // load settings for actor
        //sph_deserialise_actor_data(new_actor, actor_conf);

        actor_conf = zconfig_next(actor_conf);
    }
    // handle connections
    zconfig_t* connections = zconfig_locate(root, "connections");
    zconfig_t* con = zconfig_locate( connections, "con");
    while( con != NULL ) {
        char* conVal = zconfig_value(con);

        // Parse comma separated connection string to get the two endpoints
        int i;
        for( i = 0; conVal[i] != '\0'; ++i ) {
            if ( conVal[i] == ',' ) break;
        }

        char* output = malloc(i+1);//new char[i+1];
        char* input = malloc(strlen(conVal)-i);
        char* type[64];// = malloc(64);

        char * pch;
        pch = strtok (conVal,",");
        sprintf(output, "%s", pch);
        pch = strtok (NULL, ",");
        sprintf(input, "%s", pch);
        pch = strtok(NULL, ",");
        sprintf(type, "%s", pch);

        // Loop actors and find output actor
        for ( sphactor_t *actor = (sphactor_t *)zhash_first(actor_list); actor != NULL; actor = (sphactor_t *)zhash_next(actor_list) )
        {
            // We're the output side, so we recreate the connection
            const char *endpoint = sphactor_ask_endpoint(actor);
            if (streq(endpoint, input)) {
                /*Connection connection;
                ActorContainer* inputActor = Find(input);
                assert(inputActor);

                connection.output_node = actor;
                connection.input_node = inputActor;
                connection.input_slot = type;
                connection.output_slot = type;

                actor->connections.push_back(connection);
                inputActor->connections.push_back(connection);
                */
                sphactor_ask_connect( actor, output );
                break;
            }
        }

        con = zconfig_next(con);
        free(output);
        free(input);
    }
    zconfig_destroy(&root);
}

bool
sph_stage_clear()
{
    for ( sphactor_t *actor = (sphactor_t *)zhash_first(actor_list); actor != NULL; actor = (sphactor_t *)zhash_next(actor_list) )
    {
        sphactor_destroy(&actor);
    }

    zhash_destroy(&actor_list);
    assert(actor_list == NULL);
}

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

// TODO: this method will look in the stagedir for any library files it can load
// naming convention is <name>_sphactor.<libsuffix>
// for example log_sphactor.so
// failed attempts will be printed as errors

int register_dyn_libs(const char* path)
{
    void *actor_lib_handle;
    void (*actor_init)(void);
    char *error;

    // register stock actors
    sphactor_register( "Log", &sph_stock_log_actor, NULL, NULL );
    sphactor_register( "Count", &sph_stock_count_actor, NULL, NULL );
    sphactor_register( "Pulse", &sph_stock_pulse_actor, NULL, NULL );

    void* pyhandle = dlopen( "libpython3.9.so", RTLD_LAZY | RTLD_GLOBAL );
    assert(pyhandle);
    actor_lib_handle = dlopen("test.so", RTLD_NOW || RTLD_GLOBAL);
    if (!actor_lib_handle) {
            fprintf(stderr, "%s\n", dlerror());
            exit(EXIT_FAILURE);
    }
    dlerror(); /* Clear any existing error */
    *(void **) (&actor_init) = dlsym(actor_lib_handle, "actor_init");
    error = dlerror();
    if (error != NULL) {
        zsys_error("%s\n", error);
        exit(EXIT_FAILURE);
    }

    (*actor_init)();
    //unload
    //printf("%d\n", (*plus_one)(23));
    //dlclose(handle);
    //exit(EXIT_SUCCESS);
}

int main (int argc, char *argv [])
{
    bool verbose = false;
    zargs_t *args = zargs_new(argc, argv);
    assert(args);
    zsys_init();
    if ( zargs_arguments(args) == 0 )
        return print_help();

    if ( zargs_hasx (args, "--help", "-h", NULL) )
        return print_help();

    if (zargs_hasx(args, "--verbose", "-v", NULL) )
        verbose = true;

    //  Insert main code here
    if (verbose)
        zsys_info ("sph - ");

    register_dyn_libs("bla");
    const char *conffile = zargs_first(args);
    sphactor_register("hello", &hello_sphactor, NULL, NULL);
    sph_stage_load(conffile);

    while (!zsys_interrupted)
    {
        zclock_sleep(300);
    }
    sph_stage_clear();
    zsys_info("EXIT");
    zargs_destroy(&args);
    return 0;
}
