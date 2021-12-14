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
//#include <dlfcn.h>

//  (forward declare)
void sphactor_actor_run(zsock_t *pipe, void *args);

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

static void
s_self_switch (zsock_t *input, zsock_t *output)
{
    //  We use the low-level libzmq API for best performance
    void *zmq_input = zsock_resolve (input);
    void *zmq_output = zsock_resolve (output);
    //void *zmq_capture = self->capture? zsock_resolve (self->capture): NULL;

    zmq_msg_t msg;
    zmq_msg_init (&msg);
    while (true) {
        if (zmq_recvmsg (zmq_input, &msg, ZMQ_DONTWAIT) == -1)
            break;      //  Presumably EAGAIN
        int send_flags = zsock_rcvmore (input)? ZMQ_SNDMORE: 0;
        /*if (zmq_capture) {
            zmq_msg_t dup;
            zmq_msg_init (&dup);
            zmq_msg_copy (&dup, &msg);
            if (zmq_sendmsg (zmq_capture, &dup, send_flags) == -1)
                zmq_msg_close (&dup);
        }*/
        if (zmq_sendmsg (zmq_output, &msg, send_flags) == -1) {
            zmq_msg_close (&msg);
            break;
        }
    }
}

static int
s_run_actor_by_type(const char *actor_type, zsock_t *pipe, const char *name, zuuid_t *uuid)
{
    zhash_t *actors_reg = sphactor_get_registered();
    assert(actors_reg); // make sure something has ever been registered
    sphactor_funcs_t *funcs = (sphactor_funcs_t *)zhash_lookup( actors_reg, actor_type);
    if ( funcs == NULL )
    {
        zsys_error("%s type does not exist as a registered actor type", actor_type);
        return -1;
    }
    // run constructor if any
    void *instance = NULL;
    if ( funcs->constructor )
        instance = funcs->constructor(funcs->constructor_args);

    sphactor_shim_t shim = { funcs->handler, instance, uuid, name };
    sphactor_actor_t *act = sphactor_actor_new(pipe, (void *)&shim);
    sphactor_actor_start(act);
    // this will block until finished
    while (!zsys_interrupted)
    {
        sphactor_actor_run_once(act);
    }
    return 0;
}

int main (int argc, char *argv [])
{
    bool verbose = false;
    zargs_t *args = zargs_new(argc, argv);
    assert(args);
    zsys_init();
    //if ( zargs_arguments(args) == 0 )
    //    return print_help();

    if ( zargs_hasx (args, "--help", "-h", NULL) )
        return print_help();

    if (zargs_hasx(args, "--verbose", "-v", NULL) )
        verbose = true;

    //register_dyn_libs("bla");
    // register stock actors
    sph_stock_register_all();

    int rc = 0;
    const char *act = zargs_get(args, "--actor");
    if ( act )
    {
        zsock_t *ctrlsock = zsock_new(ZMQ_DEALER);
        assert(ctrlsock);
        //zsock_set_identity(ctrlsock, "MEPCR");
        rc = zsock_connect(ctrlsock, "tcp://127.0.0.1:4321");
        assert( rc != -1);
        const char *ctrlendp = zsock_endpoint(ctrlsock);

        rc = s_run_actor_by_type(act, ctrlsock, NULL, NULL);
        zsock_signal (ctrlsock, 0);
        zsock_destroy(&ctrlsock);
    }
    else
    {
        if (verbose)
            zsys_info ("sph - ");

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
    }

    sphactor_dispose();
    return rc;
}
