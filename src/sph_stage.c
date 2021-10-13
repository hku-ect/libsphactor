/*  =========================================================================
    sph_stage - class description

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
    sph_stage -
@discuss
@end
*/

#include "sphactor_classes.h"

//  Structure of our class

struct _sph_stage_t {
    char*           name;       //  Stage name
    char*           config_path;//  Stage file config path
    zhash_t*        actors;     //  Loaded actors
};


//  --------------------------------------------------------------------------
//  Create a new sph_stage

sph_stage_t *
sph_stage_new (const char *stage_name)
{
    sph_stage_t *self = (sph_stage_t *) zmalloc (sizeof (sph_stage_t));
    assert (self);
    //  Initialize class properties here
    self->name = strdup(stage_name);
    self->config_path = NULL;
    self->actors = zhash_new();
    assert(self->actors);
    return self;
}


//  --------------------------------------------------------------------------
//  Destroy the sph_stage

void
sph_stage_destroy (sph_stage_t **self_p)
{
    assert (self_p);
    if (*self_p) {
        sph_stage_t *self = *self_p;
        //  Free class properties here
        if (self->name) zstr_free(&self->name);
        if (self->config_path) zstr_free(&self->config_path);
        sph_stage_clear(self);
        zhash_destroy(&self->actors);
        //  Free object itself
        free (self);
        *self_p = NULL;
    }
}

int
sph_stage_cnf_load(sph_stage_t *self, const zconfig_t *cnf)
{
    assert(cnf);

    // TODO: retrieve stage name???

    zconfig_t* actors_conf = zconfig_locate((zconfig_t *)cnf, "actors");
    assert(actors_conf);
    zconfig_t* actor_conf = zconfig_locate(actors_conf, "actor");
    assert(actor_conf);
    // create actors
    while( actor_conf != NULL )
    {
        sphactor_t *new_actor =  sphactor_load(actor_conf);
        assert(new_actor);

        // save actor
        int rc = zhash_insert(self->actors, zuuid_str(sphactor_ask_uuid(new_actor)), new_actor);
        assert( rc == 0);

        // load settings for actor
        //sph_deserialise_actor_data(new_actor, actor_conf);

        actor_conf = zconfig_next(actor_conf);
    }
    // handle connections
    zconfig_t* connections = zconfig_locate((zconfig_t *)cnf, "connections");
    zconfig_t* con = zconfig_locate( connections, "con");
    while( con != NULL ) {
        char* conVal = strdup(zconfig_value(con)); // strok modifies the string and this messes up the config so dup it

        // Parse comma separated connection string to get the two endpoints
        int i;
        for( i = 0; conVal[i] != '\0'; ++i ) {
            if ( conVal[i] == ',' ) break;
        }

        char output[256];// = (char *)malloc(i+1);//new char[i+1];
        char input[256];// = (char *)malloc((strlen(conVal) + 4) - i);
        char type[64];// = malloc(64);

        char *pch = strtok (conVal, ",");
        assert(pch);
        sprintf(output, "%s", pch);
        pch = strtok (NULL, ",");
        assert(pch);
        sprintf(input, "%s", pch);
        pch = strtok(NULL, ",");
        assert(pch);
        sprintf(type, "%s", pch);

        // Loop actors and find output actor
        for ( sphactor_t *actor = (sphactor_t *)zhash_first(self->actors); actor != NULL; actor = (sphactor_t *)zhash_next(self->actors) )
        {
            // We're the output side, so we recreate the connection
            const char *endpoint = sphactor_ask_endpoint(actor);
            if (streq(endpoint, input))
            {
                int rc = sphactor_ask_connect( actor, output );
                assert(rc == 0);
                break;
            }
        }

        con = zconfig_next(con);
        free(conVal);
    }
    return zhash_size(self->actors);
}

int
sph_stage_load(sph_stage_t *self, const char *config_path)
{
    assert(self);
    assert(config_path);
    self->config_path = strdup(config_path);
    zconfig_t* root = zconfig_load(config_path);
    if ( root == NULL )
    {
        zsys_error("Error loading %s", config_path);
        return -1;
    }
    int rc = sph_stage_cnf_load(self, root);
    zconfig_destroy(&root);
    return rc;
}

int
sph_stage_clear(sph_stage_t* self)
{
    assert(self);
    for ( sphactor_t *actor = (sphactor_t *)zhash_first(self->actors); actor != NULL; actor = (sphactor_t *)zhash_next(self->actors) )
    {
        if (actor)
            sphactor_destroy(&actor);
    }

    zhash_destroy(&self->actors);
    assert(self->actors == NULL);
    self->actors = zhash_new();
    return 0;
}

static zconfig_t *
s_sph_stage_save_zconfig(sph_stage_t *self)
{
    zconfig_t* config = sphactor_zconfig_new("root");
    for (sphactor_t *it = (sphactor_t *)zhash_first(self->actors); it != NULL; it = (sphactor_t *)zhash_next( self->actors ) )
    {
        zconfig_t* actorSection = sphactor_zconfig_append(it, config);

        // Add custom actor data to section
        //actor->SerializeActorData(actorSection);

        zconfig_t* connections = zconfig_locate(config, "connections");
        if ( connections == NULL ) {
            connections = zconfig_new("connections", config);
        }

        for (char *c = (char *)zlist_first(sphactor_connections(it)); c != (char *)NULL; c = (char *)zlist_next(sphactor_connections(it)) )
        {
            zconfig_t* item = zconfig_new( "con", connections );
            assert( item );
            zconfig_set_value(item,"%s,%s,%s", sphactor_ask_endpoint(it), c, "OSC" );
        }
    }
    return config;
}

int
sph_stage_save(sph_stage_t *self)
{
    assert(self);
    assert(self->config_path);
    return sph_stage_save_as(self, self->config_path);
}

int
sph_stage_save_as(sph_stage_t *self, const char *config_path)
{
    assert(self);
    assert(config_path);
    zconfig_t* config = s_sph_stage_save_zconfig(self);
    int rc = zconfig_save(config, config_path);
    assert(rc == 0);
    zstr_free(&self->config_path);
    self->config_path = strdup(config_path);
    return rc;
}

const zhash_t *
sph_stage_actors(sph_stage_t *self)
{
    assert(self);
    return self->actors;
}

sphactor_t *
sph_stage_find_actor (sph_stage_t *self, const char *id)
{
    assert(self);
    assert(self->actors);
    sphactor_t *actor = (sphactor_t *)zhash_lookup(self->actors, id);
    return actor;
}

int
sph_stage_add_actor(sph_stage_t *self, sphactor_t *actor)
{
    assert(self);
    assert(actor);

    int rc = zhash_insert(self->actors, zuuid_str(sphactor_ask_uuid(actor)), actor);
    return rc;
}

int
sph_stage_remove_actor(sph_stage_t *self, const char *actor_id)
{
    assert(self);
    assert(actor_id);

    sphactor_t *actor = sph_stage_find_actor(self, actor_id);
    if (actor)
    {
        zhash_delete(self->actors, actor_id);
        sphactor_destroy(&actor);
        return 0;
    }
    return -1;
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
sph_stage_test (bool verbose)
{
    printf (" * sph_stage: ");

    //  @selftest
    //  Simple create/destroy test
    zsys_init();
    sph_stage_t *self = sph_stage_new ("test");
    assert (self);
    sph_stage_destroy (&self);

    char *cnfstr =
    "actors\n"
    "    actor\n"
    "        uuid = \"7B21D87CB6B04FC5801A5B396269876D\"\n"
    "        type = \"Log\"\n"
    "        name = \"8FADA7\"\n"
    "        endpoint = \"inproc://8FADA7E8835E42D5AF85FC75F4A9B70E\"\n"
    "        xpos = \"502.500000\"\n"
    "        ypos = \"312.000000\"\n"
    "    actor\n"
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
    "    con = \"inproc://2A7110DFC47C4DF19EB1D17E390CF86B,inproc://7B21D87CB6B04FC5801A5B396269876D,OSC\"\n";
    ///"    con = \"inproc://7B21D87CB6B04FC5801A5B396269876D,inproc://8FADA7E8835E42D5AF85FC75F4A9B70E,OSC\"\n"
    zconfig_t *root = zconfig_str_load (cnfstr);
    sph_stock_register_all();

    // test amount of loaded actors returned
    sph_stage_t *stage = sph_stage_new("test");
    int rc = sph_stage_cnf_load(stage, root );
    assert( rc == 2);
    zclock_sleep(30);
    rc = sph_stage_clear(stage);
    assert( rc == 0);
    sph_stage_destroy(&stage);

    zclock_sleep(30); // some time for cleanup

    // some stage tests
    sph_stage_t *stage2 = sph_stage_new("test");
    rc = sph_stage_cnf_load(stage2, root );
    assert( rc == 2);
    zclock_sleep(30);
    sphactor_t *pulseact = sph_stage_find_actor(stage2, "2A7110DFC47C4DF19EB1D17E390CF86B");
    assert(pulseact);
    assert( streq( zuuid_str(sphactor_ask_uuid(pulseact)), "2A7110DFC47C4DF19EB1D17E390CF86B" ));
    assert( streq( sphactor_ask_actor_type(pulseact), "Pulse"));
    // save stage
    zconfig_t *testsave = s_sph_stage_save_zconfig(stage2);
    assert(testsave);
    //char *testsavestr = zconfig_str_save(testsave);
    //assert(streq(cnfstr, testsavestr));
    zconfig_t *saveactors = zconfig_locate(testsave, "actors/actor");
    assert(saveactors);
    zconfig_t *testkey = zconfig_locate(saveactors, "uuid");
    assert(testkey);
    testkey = zconfig_locate(saveactors, "type");
    assert(testkey);
    assert(streq(zconfig_value(testkey), "Pulse") || streq(zconfig_value(testkey), "Log") );
    testkey = zconfig_locate(saveactors, "name");
    assert(testkey);
    assert(streq(zconfig_value(testkey), "8FADA7") || streq(zconfig_value(testkey), "2A7110") );
    testkey = zconfig_locate(saveactors, "endpoint");
    assert(testkey);
    assert(streq(zconfig_value(testkey), "inproc://2A7110DFC47C4DF19EB1D17E390CF86B") || streq(zconfig_value(testkey), "inproc://8FADA7E8835E42D5AF85FC75F4A9B70E") );
    testkey = zconfig_locate(saveactors, "xpos");
    assert(testkey);
    assert(streq(zconfig_value(testkey), "502.500000") || streq(zconfig_value(testkey), "34.500000") );
    testkey = zconfig_locate(saveactors, "ypos");
    assert(testkey);
    assert(streq(zconfig_value(testkey), "312.000000") || streq(zconfig_value(testkey), "426.000000") );
    saveactors = zconfig_next(saveactors);
    assert(saveactors);
    testkey = zconfig_locate(saveactors, "uuid");
    assert(testkey);
    testkey = zconfig_locate(saveactors, "type");
    assert(testkey);
    testkey = zconfig_locate(saveactors, "name");
    assert(testkey);
    testkey = zconfig_locate(saveactors, "endpoint");
    assert(testkey);
    testkey = zconfig_locate(saveactors, "xpos");
    assert(testkey);
    testkey = zconfig_locate(saveactors, "ypos");
    assert(testkey);
    zconfig_destroy(&testsave);

    // remove test
    sph_stage_remove_actor( stage2, "2A7110DFC47C4DF19EB1D17E390CF86B" );
    pulseact = sph_stage_find_actor(stage2, "2A7110DFC47C4DF19EB1D17E390CF86B");
    assert(pulseact == NULL);
    sph_stage_destroy(&stage2);
    zconfig_destroy( &root );

    sph_stage_t *stage3 = sph_stage_new("test_add");
    assert(stage3);
    sphactor_t *testact = sphactor_new_by_type("Log", NULL, NULL);
    assert(testact);
    rc = sph_stage_add_actor(stage3, testact);
    assert(rc == 0);

    const zhash_t *actors = sph_stage_actors(stage3);
    assert( zhash_size((zhash_t *)actors) == 1);
    const sphactor_t *act2 =  (const sphactor_t *)zhash_first((zhash_t *)actors);
    assert( testact == (sphactor_t *)act2 );
    rc = sph_stage_remove_actor(stage3, zuuid_str( sphactor_ask_uuid(testact)) );
    assert( rc == 0 );
    assert( zhash_size((zhash_t *)actors) == 0 );
    sph_stage_clear(stage3);
    sph_stage_destroy(&stage3);

    zsys_shutdown();

    //  @end
    printf ("OK\n");
}
