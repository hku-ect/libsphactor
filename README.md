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
git clone http://github.com/aaronvark/libsphactor.git
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
