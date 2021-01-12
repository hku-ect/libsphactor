#ifndef LIBSPHACTOR_HPP_INCLUDED
#define LIBSPHACTOR_HPP_INCLUDED

#include "libsphactor.h"

template<class SphactorClass>
SPHACTOR_EXPORT static zmsg_t *
sphactor_member_handler(sphactor_event_t *ev, void *args)
{
    assert(args);
    SphactorClass *self = static_cast<SphactorClass*>(args);
    return self->handleMsg(ev);
};

template<class SphactorClass>
SPHACTOR_EXPORT static void *
sphactor_cpp_constructor(void *arg)
{
    return new SphactorClass();
};

template<class SphactorClass>
SPHACTOR_EXPORT static void *
sphactor_cpp_arg_constructor(void *arg)
{
    return new SphactorClass(arg);
};

template<class SphactorClass>
SPHACTOR_EXPORT int
sphactor_register (const char *actor_type)
{
    return sphactor_register(actor_type, sphactor_member_handler<SphactorClass>, &sphactor_cpp_constructor<SphactorClass>, NULL);
}

template<class SphactorClass>
SPHACTOR_EXPORT int
sphactor_register (const char *actor_type, void *constructor_arg)
{
    return sphactor_register(actor_type, sphactor_member_handler<SphactorClass>, &sphactor_cpp_arg_constructor<SphactorClass>, constructor_arg);
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
        if ( streq(ev->type, "INIT") )
        {
            return this->handleInit(ev);
        }
        else if ( streq(ev->type, "TIME") )
        {
            return this->handleTimer(ev);
        }
        else if ( streq(ev->type, "API") )
        {
            return this->handleAPI(ev);
        }
        else if ( streq(ev->type, "SOCK") )
        {
            return this->handleSocket(ev);
        }
        else if ( streq(ev->type, "SOCKFD") )
        {
            zframe_t *frame = zmsg_pop(ev->msg);
            assert(frame);
            zmsg_destroy(&ev->msg);

            if (zframe_size(frame) == sizeof( void *) )
            {
                void *p = *(reinterpret_cast<void **>(zframe_data(frame)));
                zsock_t *sock = static_cast<zsock_t *>( zsock_resolve(p) );
                if ( sock )
                {
                    zmsg_t *msg = zmsg_recv(sock);
                    assert(msg);
                    ev->msg = msg;
                }
            }
            zframe_destroy(&frame);

            return this->handleCustomSocket(ev);
        }
        else if ( streq(ev->type, "STOP") )
        {
            return this->handleStop(ev);
        }
        else if ( streq(ev->type, "DESTROY") )
        {
            delete this;
            return nullptr;
        }
        else
            zsys_error("Unhandled sphactor event: %s", ev->type);
    }
};


#endif // LIBSPHACTOR_HPP_INCLUDED
