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

static int
print_help()
{
    puts ("sph [options] <config file>");
    puts ("  --verbose / -v         verbose output");
    puts ("  --help / -h            this information");
    puts ("  <config file>          stage config file to load");
    return 0;
}

// TODO: this method will look in the stagedir for any library files it can load
// naming convention is <name>_sphactor.<libsuffix>
// for example log_sphactor.so
// failed attempts will be printed as errors

/*int register_dyn_libs(const char* path)
{
    void *actor_lib_handle;
    void (*actor_init)(void);
    char *error;

    void* pyhandle = dlopen( "libpython3.9.so", RTLD_LAZY | RTLD_GLOBAL );
    assert(pyhandle);
    actor_lib_handle = dlopen("test.so", RTLD_NOW || RTLD_GLOBAL);
    if (!actor_lib_handle) {
            fprintf(stderr, "%s\n", dlerror());
            exit(EXIT_FAILURE);
    }
    dlerror(); //  Clear any existing error
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
}*/

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

    //register_dyn_libs("bla");
    // register stock actors
    sphactor_register( "Log", &sph_stock_log_actor, NULL, NULL );
    sphactor_register( "Count", &sph_stock_count_actor, NULL, NULL );
    sphactor_register( "Pulse", &sph_stock_pulse_actor, NULL, NULL );

    const char *conffile = zargs_first(args);

    sph_stage_t *stage = sph_stage_new("clistage");
    sph_stage_load(stage, conffile);

    while (!zsys_interrupted)
    {
        zclock_sleep(300);
    }
    sph_stage_clear(stage);
    zsys_info("EXIT");
    zargs_destroy(&args);
    return 0;
}
