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
        zhash_destroy(&self->actors);
        //  Free object itself
        free (self);
        *self_p = NULL;
    }
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
sph_stage_cnf_load(sph_stage_t *self, const zconfig_t *cnf)
{
    assert(cnf);

    // TODO: retrieve stage name???

    zconfig_t* actors_conf = zconfig_locate(cnf, "actors");
    assert(actors_conf);
    zconfig_t* actor_conf = zconfig_locate(actors_conf, "actor");
    assert(actor_conf);
    // create actors
    while( actor_conf != NULL )
    {
        sphactor_t *new_actor =  sphactor_load(actor_conf);
        assert(new_actor);

        // save actor
        int rc = zhash_insert(self->actors, sphactor_ask_uuid(new_actor), new_actor);
        assert( rc == 0);

        // load settings for actor
        //sph_deserialise_actor_data(new_actor, actor_conf);

        actor_conf = zconfig_next(actor_conf);
    }
    // handle connections
    zconfig_t* connections = zconfig_locate(cnf, "connections");
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
        free(output);
        free(input);
    }
    return zhash_size(self->actors);
}

int
sph_stage_clear(sph_stage_t* self)
{
    assert(self);
    for ( sphactor_t *actor = (sphactor_t *)zhash_first(self->actors); actor != NULL; actor = (sphactor_t *)zhash_next(self->actors) )
    {
        sphactor_destroy(&actor);
    }

    zhash_destroy(&self->actors);
    assert(self->actors == NULL);
    self->actors = zhash_new();
    return 0;
}

int
sph_stage_save(sph_stage_t *self)
{
    assert(self);
    assert(self->config_path);
    // TODO: create zconfig
    zconfig_t* config = zconfig_new("root", NULL);
    zconfig_save(config, self->config_path);
}

int
sph_stage_save_as(sph_stage_t *self, const char *config_path)
{
    assert(self);
    assert(config_path);
    // TODO: create zconfig
    zconfig_t* config = zconfig_new("root", NULL);
    int rc = zconfig_save(config, config_path);
    assert(rc == 0);
    zstr_free(self->config_path);
    self->config_path = config_path;
    return rc;
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

    zconfig_t *root = zconfig_str_load (
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
    "    con = \"inproc://2A7110DFC47C4DF19EB1D17E390CF86B,inproc://7B21D87CB6B04FC5801A5B396269876D,OSC\"\n"
    ///"    con = \"inproc://7B21D87CB6B04FC5801A5B396269876D,inproc://8FADA7E8835E42D5AF85FC75F4A9B70E,OSC\"\n"
    );
    int rc = sphactor_register("Log", sph_stock_log_actor, NULL, NULL);
    assert(rc == 0);
    rc = sphactor_register("Pulse", sph_stock_pulse_actor, NULL, NULL);
    assert(rc == 0);

    sph_stage_t *stage = sph_stage_new("test");
    rc = sph_stage_cnf_load(stage, root );
    assert( rc == 2);
    zconfig_destroy(&root);
    sph_stage_clear(stage);
    zclock_sleep(3000);
    sph_stage_destroy(&stage);
    zsys_shutdown();

    //  @end
    printf ("OK\n");
}
