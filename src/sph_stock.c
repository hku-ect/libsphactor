/*  =========================================================================
    sph_stock - class description

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
    sph_stock -
@discuss
@end
*/

#include "sphactor_classes.h"

const char * logCapabilities = "inputs\n"
                               "    input\n"
                               "        type = \"OSC\"\n";


zmsg_t *
sph_stock_log_actor( sphactor_event_t *ev, void* args )
{
    if ( streq(ev->type, "INIT")) {
        sphactor_actor_set_capability((sphactor_actor_t*)ev->actor, zconfig_str_load(logCapabilities));
    }
    else
    if ( streq(ev->type, "SOCK")) {
        if ( ev->msg == NULL ) return NULL;

        zframe_t* frame = NULL;

        do {
            frame = zmsg_pop(ev->msg);
            if ( frame ) {
                char* msg;
                // parse zosc_t msg
                zosc_t * oscMsg = zosc_fromframe(frame);
                if ( oscMsg )
                {
                    zosc_print(oscMsg);
                    fflush(stdout);
                }


                zosc_destroy(&oscMsg);
            }
        } while (frame != NULL );

        zframe_destroy(&frame);
        zmsg_destroy(&ev->msg);

        return NULL;
    }

    return ev->msg;
}


const char * countCapabilities = "inputs\n"
                                "    input\n"
                                "        type = \"OSC\"\n"
                                "outputs\n"
                                "    output\n"
                                "        type = \"OSC\"\n";

zmsg_t *
sph_stock_count_actor( sphactor_event_t *ev, void* args )
{
    static int sph_stock_count_actor_count = 0;
    if ( streq(ev->type, "INIT")) {
        sphactor_actor_set_capability((sphactor_actor_t*)ev->actor, zconfig_str_load(countCapabilities));
    }
    else
    if ( streq(ev->type, "SOCK")) {
        sph_stock_count_actor_count++; // increment counter

        // set custom report
        zosc_t * msg = zosc_create("/report", "si",
                                   "counter", (int32_t)sph_stock_count_actor_count);

        sphactor_actor_set_custom_report_data( (sphactor_actor_t*)ev->actor, msg );
    }
    return ev->msg;
}


const char * pulseCapabilities =
                                "capabilities\n"
                                "    data\n"
                                "        name = \"timeout\"\n"
                                "        type = \"int\"\n"
                                "        value = \"1000\"\n"
                                "        min = \"1\"\n"
                                "        max = \"10000\"\n"
                                "        step = \"1\"\n"
                                "        api_call = \"SET TIMEOUT\"\n"
                                "        api_value = \"i\"\n"           // optional picture format used in zsock_send
                                "outputs\n"
                                "    output\n"
                                "        type = \"OSC\"\n";

zmsg_t *
sph_stock_pulse_actor( sphactor_event_t *ev, void* args )
{
    static int sph_stock_count_actor_count = 0;
    if ( streq(ev->type, "INIT")) {
        sphactor_actor_set_capability((sphactor_actor_t*)ev->actor, zconfig_str_load(pulseCapabilities));
    }
    else
    if ( streq(ev->type, "TIME")) {

        zosc_t * osc = zosc_create("/pulse", "s", "PULSE");

        zmsg_t *msg = zmsg_new();
        zframe_t *frame = zframe_new(zosc_data(osc), zosc_size(osc));
        zmsg_append(msg, &frame);

        // clean up
        zframe_destroy(&frame);
        zmsg_destroy(&ev->msg);
        zosc_destroy(&osc);

        // publish new msg
        return msg;
    }
    zmsg_destroy(&ev->msg);
    return NULL;
}

void
sph_stock_register_all (void)
{
    sphactor_register( "Log", &sph_stock_log_actor, zconfig_str_load(logCapabilities), NULL, NULL );
    sphactor_register( "Count", &sph_stock_count_actor, zconfig_str_load(countCapabilities), NULL, NULL );
    sphactor_register( "Pulse", &sph_stock_pulse_actor, zconfig_str_load(pulseCapabilities), NULL, NULL );
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
sph_stock_test (bool verbose)
{
    printf (" * sph_stock: ");

    //  @selftest
    //  Simple create/destroy test
    //sph_stock_t *self = sph_stock_new ();
    //assert (self);
    //sph_stock_destroy (&self);
    //  @end
    printf ("OK\n");
}
