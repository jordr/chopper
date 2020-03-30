//===-- Debug.h -------------------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_INTERNAL_SUPPORT_DEBUG_H
#define KLEE_INTERNAL_SUPPORT_DEBUG_H

#include <klee/Config/config.h>
#include <llvm/Support/Debug.h>

// We define wrappers around the LLVM DEBUG macros that are conditionalized on
// whether the LLVM we are building against has the symbols needed by these
// checks.

#ifdef ENABLE_KLEE_DEBUG
#define KLEE_DEBUG_WITH_TYPE(TYPE, X) DEBUG_WITH_TYPE(TYPE, X)
#else
#define KLEE_DEBUG_WITH_TYPE(TYPE, X) do { } while (0)
#endif
#define KLEE_DEBUG(X) KLEE_DEBUG_WITH_TYPE(DEBUG_TYPE, X)

#define DEBUG_BASIC "basic"
#define DEBUG_SIGNATURES "signatures"
// AutoChopper
#define DEBUG_CHOP "chop"
#define DEBUG_RECOVERY "recovery", "recoverytimers", "recovery2", "basic"
#define DEBUG_RECOVERY_VERBOSE "recovery2"
#define DEBUG_RECOVERY_TIMERS "recoverytimers"
#define DEBUG_INSTCOUNT "instcount"
#define DEBUG_MODREF "modref"
#endif

// define DEBUG_CHOPPER
#define GET_MACRO(_0,_1,_2,_3,_4,NAME,...) NAME
#define DEBUG_CHOPPER(...) GET_MACRO(__VA_ARGS__, DEBUG_CHOPPER4, DEBUG_CHOPPER3, DEBUG_CHOPPER2, DEBUG_CHOPPER1)(__VA_ARGS__)
#define DEBUG_CHOPPER1(TYPE1, X) DEBUG_WITH_TYPE(TYPE1, X)
#define DEBUG_CHOPPER2(TYPE1, TYPE2, X) {DEBUG_WITH_TYPE(TYPE2, X); DEBUG_CHOPPER1(TYPE1, X); }
#define DEBUG_CHOPPER3(TYPE1, TYPE2, TYPE3, X) {DEBUG_WITH_TYPE(TYPE3, X); DEBUG_CHOPPER2(TYPE1, TYPE2, X); }
#define DEBUG_CHOPPER4(TYPE1, TYPE2, TYPE3, TYPE4, X) {DEBUG_WITH_TYPE(TYPE4, X); DEBUG_CHOPPER3(TYPE1, TYPE2, TYPE3, X); }