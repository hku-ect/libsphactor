#ifndef SPHACTOR_HPP
#define SPHACTOR_HPP

template<class SphactorClass>
SPHACTOR_EXPORT static zmsg_t *
sphactor_member_handler(sphactor_event_t *ev, void *args)
{
    assert(args);
    SphactorClass *self = static_cast<SphactorClass*>(args);
    return self->handleMsg(ev);
};

template<class SphactorClass>
SPHACTOR_EXPORT sphactor_t *
sphactor_new ( SphactorClass *inst, const char *name=nullptr, zuuid_t *uuid=nullptr )
{
    assert(inst);
    return sphactor_new(sphactor_member_handler<SphactorClass>, inst, name, uuid);
}

// void pointer to a costructor
template<class SphactorClass>
SPHACTOR_EXPORT void *
sphactoractor_constructor()
{
    return new SphactorClass;
}

#endif // SPHACTOR_HPP
