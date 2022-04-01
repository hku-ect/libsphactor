#ifndef LIBSPHACTOR_HPP_INCLUDED
#define LIBSPHACTOR_HPP_INCLUDED

#include "libsphactor.h"

template<class SphactorClass>
static zmsg_t *
sphactor_member_handler(sphactor_event_t *ev, void *args)
{
    assert(args);
    SphactorClass *self = static_cast<SphactorClass*>(args);
    return self->handleMsg(ev);
};

template<class SphactorClass>
static void *
sphactor_cpp_constructor(void *arg)
{
    return new SphactorClass();
};

template<class SphactorClass>
static void *
sphactor_cpp_arg_constructor(void *arg)
{
    return new SphactorClass(arg);
};

template<class SphactorClass>
int
sphactor_register (const char *actor_type, const char *capability)
{
    if (capability)
        return sphactor_register(actor_type, sphactor_member_handler<SphactorClass>, zconfig_str_load(capability), &sphactor_cpp_constructor<SphactorClass>, NULL);
    return sphactor_register(actor_type, sphactor_member_handler<SphactorClass>, NULL, &sphactor_cpp_constructor<SphactorClass>, NULL);
}

template<class SphactorClass>
int
sphactor_register (const char *actor_type, const char *capability, void *constructor_arg)
{
    if (capability)
        return sphactor_register(actor_type, sphactor_member_handler<SphactorClass>, zconfig_str_load(capability), &sphactor_cpp_arg_constructor<SphactorClass>, constructor_arg);
    return sphactor_register(actor_type, sphactor_member_handler<SphactorClass>, NULL, &sphactor_cpp_arg_constructor<SphactorClass>, constructor_arg);
}

// Base class for actors, just inherit this class if convenient

class Sphactor
{
public:
    Sphactor() {}
    virtual ~Sphactor() {}

    virtual zmsg_t *
    handleInit(sphactor_event_t *ev) { if ( ev->msg ) zmsg_destroy(&ev->msg); return nullptr; }

    virtual zmsg_t *
    handleTimer(sphactor_event_t *ev) { if ( ev->msg ) zmsg_destroy(&ev->msg); return nullptr; }

    virtual zmsg_t *
    handleAPI(sphactor_event_t *ev) { if ( ev->msg ) zmsg_destroy(&ev->msg); return nullptr; }

    virtual zmsg_t *
    handleSocket(sphactor_event_t *ev) { if ( ev->msg ) zmsg_destroy(&ev->msg); return nullptr; }

    virtual zmsg_t *
    handleCustomSocket(sphactor_event_t *ev) { if ( ev->msg ) zmsg_destroy(&ev->msg); return nullptr; }

    virtual zmsg_t *
    handleStop(sphactor_event_t *ev) { if ( ev->msg ) zmsg_destroy(&ev->msg); return nullptr; }

    zmsg_t *handleMsg(sphactor_event_t *ev)
    {
        zmsg_t *ret = nullptr;
        if ( streq(ev->type, "INIT") )
        {
            ret = this->handleInit(ev);
        }
        else if ( streq(ev->type, "TIME") )
        {
            ret = this->handleTimer(ev);
        }
        else if ( streq(ev->type, "API") )
        {
            ret = this->handleAPI(ev);
        }
        else if ( streq(ev->type, "SOCK") )
        {
            ret = this->handleSocket(ev);
        }
        else if ( streq(ev->type, "FDSOCK") )
        {
            assert(ev->msg);
            ret = this->handleCustomSocket(ev);
        }
        else if ( streq(ev->type, "STOP") )
        {
            ret = this->handleStop(ev);
        }
        else if ( streq(ev->type, "DESTROY") )
        {
            delete this;
        }
        else
            zsys_error("Unhandled sphactor event: %s", ev->type);

        if (ev->msg && ret != ev->msg)
            zmsg_destroy (&ev->msg);
        return ret;
    }
};


#endif // LIBSPHACTOR_HPP_INCLUDED
