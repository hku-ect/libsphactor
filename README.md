# libsphactor

"Extended nodal actor framework based on zactor"

# Prototype Build Guide

## OSX

### Building Dependencies

There are two main dependencies:

 * libzmq
 * czmq

Dependencies for the build process / dependencies are:

   * git, libtool, autoconf, automake, cmake, make, pkg-config, pcre,

Building them should be pretty straight forward:

 * Get dependencies via brew:
```
brew install libtool autoconf automake pkg-config cmake make zeromq
```
 * Clone & build libzmq & czmq
```
git clone git://github.com/zeromq/libzmq.git
cd libzmq
./autogen.sh
./configure
make check
sudo make install
cd ..

git clone git://github.com/zeromq/czmq.git
cd czmq
./autogen.sh && ./configure && make check
sudo make install
cd ..
```

---

### Building Libsphactor

Once the above dependencies are installed, you are ready to build libsphactor. The process for this is much the same:

 * Clone the repo
```
git clone http://github.com/sphaero/libsphactor.git
```
 * Build the project
```
cd libsphactor
./autogen.sh
./configure
make check
sudo make install
```

---

### Creating an XCode project

To create an xcode project, perform the following commands from the root git folder:

```
mkdir xcodeproj
cd xcodeproj
cmake -G Xcode ..
```
This should generate a valid Xcode project that can run and pass tests.


## Debian/Ubuntu Linux

(tested on ubuntu 16.04)

Install dependencies

```
sudo apt-get update
sudo apt-get install -y \
    build-essential libtool \
    pkg-config autotools-dev autoconf automake \
    uuid-dev libpcre3-dev libsodium-dev libzmq5-dev libczmq-dev
```

Clone this repo, build and install

```
git clone https://github.com/hku-ect/libsphactor.git
cd libsphactor
./autogen.sh
./configure && make
sudo make install
```
You can now use libsphactor as a static (/usr/local/lib/libsphactor.a) or dynamic lib (/usr/local/lib/libsphactor.so). Include file are in /usr/local/include.

### Minimal example app

test.c:
```c
#include <sphactor_library.h>
#include <czmq.h>

//  An actor is a method wich receives a sphactor_event as an argument
//  and returns a zmsg_t. Zmsg_t is just a message we can send.
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

//  This is identical to the first hello_actor
static zmsg_t *
hello_sphactor2(sphactor_event_t *ev, void *args)
{
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

int main()
{
    //  create two actors
    sphactor_t *hello1 = sphactor_new ( hello_sphactor, NULL, NULL, NULL);
    sphactor_t *hello2 = sphactor_new ( hello_sphactor2, NULL, NULL, NULL);

    //  connect the two actors to each other
    sphactor_connect(hello1, sphactor_endpoint(hello2));
    sphactor_connect(hello2, sphactor_endpoint(hello1));

    //  send multiple strings to the first actor
    zstr_sendm( sphactor_socket(hello1), "SEND");  // this is an API command
    zstr_sendm( sphactor_socket(hello1), "HELLO"); // first string
    zstr_sendm( sphactor_socket(hello1), "WORLD"); // etc...
    zstr_sendm( sphactor_socket(hello1), "AND");
    zstr_sendm( sphactor_socket(hello1), "ALIEN");
    zstr_send( sphactor_socket(hello1), "SPACELINGS"); // finish message construction and send the message

    zclock_sleep(10); //  give some time for the test to complete, since it's threaded

    sphactor_destroy (&hello1);
    sphactor_destroy (&hello2);

    return 0;
}

```
Build this file and run:

```
gcc -o test main.c -lczmq -lsphactor
./test
```

If correct it wil show:
```
I: 19-12-03 13:20:49 Hello2 actor A8D382 says: HELLO
I: 19-12-03 13:20:49 Hello actor C3FA9D says: WORLD
I: 19-12-03 13:20:49 Hello2 actor A8D382 says: AND
I: 19-12-03 13:20:49 Hello actor C3FA9D says: ALIEN
I: 19-12-03 13:20:49 Hello2 actor A8D382 says: SPACELINGS
```

You can also use the static lib instead of the dynamic one:
```
gcc -o test main.c /usr/local/lib/libsphactor.a /usr/lib/x86_64-linux-gnu/libczmq.a -l zmq -lpthread
```
