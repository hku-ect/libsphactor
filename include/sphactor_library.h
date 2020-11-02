/*  =========================================================================
    libsphactor - generated layer of public API

    Copyright (c) 2020 the Contributors as noted in the AUTHORS file.

    This file is part of Sphactor, an open-source framework for high level
    actor model concurrency --- http://sphactor.org

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.

################################################################################
#  THIS FILE IS 100% GENERATED BY ZPROJECT; DO NOT EDIT EXCEPT EXPERIMENTALLY  #
#  Read the zproject/README.md for information about making permanent changes. #
################################################################################
    =========================================================================
*/

#ifndef SPHACTOR_LIBRARY_H_INCLUDED
#define SPHACTOR_LIBRARY_H_INCLUDED

//  Set up environment for the application

//  External dependencies
#ifndef CZMQ_BUILD_DRAFT_API
#define CZMQ_BUILD_DRAFT_API
#endif
#include <czmq.h>

//  SPHACTOR version macros for compile-time API detection
#define SPHACTOR_VERSION_MAJOR 0
#define SPHACTOR_VERSION_MINOR 1
#define SPHACTOR_VERSION_PATCH 0

#define SPHACTOR_MAKE_VERSION(major, minor, patch) \
    ((major) * 10000 + (minor) * 100 + (patch))
#define SPHACTOR_VERSION \
    SPHACTOR_MAKE_VERSION(SPHACTOR_VERSION_MAJOR, SPHACTOR_VERSION_MINOR, SPHACTOR_VERSION_PATCH)

#if defined (__WINDOWS__)
#   if defined SPHACTOR_STATIC
#       define SPHACTOR_EXPORT
#   elif defined SPHACTOR_INTERNAL_BUILD
#       if defined DLL_EXPORT
#           define SPHACTOR_EXPORT __declspec(dllexport)
#       else
#           define SPHACTOR_EXPORT
#       endif
#   elif defined SPHACTOR_EXPORTS
#       define SPHACTOR_EXPORT __declspec(dllexport)
#   else
#       define SPHACTOR_EXPORT __declspec(dllimport)
#   endif
#   define SPHACTOR_PRIVATE
#elif defined (__CYGWIN__)
#   define SPHACTOR_EXPORT
#   define SPHACTOR_PRIVATE
#else
#   if (defined __GNUC__ && __GNUC__ >= 4) || defined __INTEL_COMPILER
#       define SPHACTOR_PRIVATE __attribute__ ((visibility ("hidden")))
#       define SPHACTOR_EXPORT __attribute__ ((visibility ("default")))
#   else
#       define SPHACTOR_PRIVATE
#       define SPHACTOR_EXPORT
#   endif
#endif

//  Opaque class structures to allow forward references
//  These classes are stable or legacy and built in all releases
typedef struct _sphactor_t sphactor_t;
#define SPHACTOR_T_DEFINED
typedef struct _sphactor_actor_t sphactor_actor_t;
#define SPHACTOR_ACTOR_T_DEFINED
typedef struct _sphactor_report_t sphactor_report_t;
#define SPHACTOR_REPORT_T_DEFINED


//  Public classes, each with its own header file
#include "sphactor.h"
#include "sphactor_actor.h"
#include "sphactor_report.h"

#ifdef SPHACTOR_BUILD_DRAFT_API

#ifdef __cplusplus
extern "C" {
#endif

//  Self test for private classes
SPHACTOR_EXPORT void
    sphactor_private_selftest (bool verbose, const char *subtest);

#ifdef __cplusplus
}
#endif
#endif // SPHACTOR_BUILD_DRAFT_API

#endif
/*
################################################################################
#  THIS FILE IS 100% GENERATED BY ZPROJECT; DO NOT EDIT EXCEPT EXPERIMENTALLY  #
#  Read the zproject/README.md for information about making permanent changes. #
################################################################################
*/
