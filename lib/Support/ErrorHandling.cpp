//===-- ErrorHandling.cpp -------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "klee/Internal/Support/ErrorHandling.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/raw_ostream.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>

#include <set>

using namespace klee;

FILE *klee::klee_warning_file = NULL;
FILE *klee::klee_message_file = NULL;
std::vector<unsigned>* klee::klee_warning_filter = NULL;

static const char *warningPrefix = "WARNING";
static const char *warningOncePrefix = "WARNING ONCE";
static const char *errorPrefix = "ERROR";
static const char *notePrefix = "NOTE";

static bool shouldSetColor(const char *pfx, const char *msg,
                           const char *prefixToSearchFor) {
  if (pfx && strcmp(pfx, prefixToSearchFor) == 0)
    return true;

  if (llvm::StringRef(msg).startswith(prefixToSearchFor))
    return true;

  return false;
}

static unsigned long klee_cstrhash(const char *str)
{
  unsigned long hash = 5381;
  int c;

  while (c = *str++)
      hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

  return hash;
}

static void klee_vfmessage(FILE *fp, const char *pfx, const char *msg,
                           va_list ap) {
  if (!fp)
    return;

  llvm::raw_fd_ostream fdos(fileno(fp), /*shouldClose=*/false,
                            /*unbuffered=*/true);

  // JOR
  unsigned hash = klee_cstrhash(msg)%0xffffff;
  if(pfx && !strcmp(pfx, warningPrefix) && klee_warning_filter && !(std::find(klee_warning_filter->begin(), klee_warning_filter->end(), hash) != klee_warning_filter->end()))
    return;

  fdos.changeColor(llvm::raw_ostream::BLACK, false, true);
  fdos.changeColor(llvm::raw_ostream::BLACK, false, false);
  fdos << "[";
  fdos.write_hex(klee_cstrhash(msg)%0xffffff);
  fdos << "]";
  fdos.resetColor();
  fdos << " ";

  bool modifyConsoleColor = fdos.is_displayed() && (fp == stderr);

  if (modifyConsoleColor) {

    // Warnings
    if (shouldSetColor(pfx, msg, warningPrefix))
      fdos.changeColor(llvm::raw_ostream::MAGENTA,
                       /*bold=*/false,
                       /*bg=*/false);

    // Once warning
    if (shouldSetColor(pfx, msg, warningOncePrefix))
      fdos.changeColor(llvm::raw_ostream::MAGENTA,
                       /*bold=*/true,
                       /*bg=*/false);

    // Errors
    if (shouldSetColor(pfx, msg, errorPrefix))
      fdos.changeColor(llvm::raw_ostream::RED,
                       /*bold=*/true,
                       /*bg=*/false);

    // Notes
    if (shouldSetColor(pfx, msg, notePrefix))
      fdos.changeColor(llvm::raw_ostream::WHITE,
                       /*bold=*/true,
                       /*bg=*/false);
  }

  fdos << "KLEE: ";
  if (pfx)
    fdos << pfx << ": ";

  // FIXME: Can't use fdos here because we need to print
  // a variable number of arguments and do substitution
  vfprintf(fp, msg, ap);
  fflush(fp);

  fdos << "\n";

  if (modifyConsoleColor)
    fdos.resetColor();

  fdos.flush();
}

/* Prints a message/warning.

   If pfx is NULL, this is a regular message, and it's sent to
   klee_message_file (messages.txt).  Otherwise, it is sent to
   klee_warning_file (warnings.txt).

   Iff onlyToFile is false, the message is also printed on stderr.
*/
static void klee_vmessage(const char *pfx, bool onlyToFile, const char *msg,
                          va_list ap) {
  if (!onlyToFile) {
    va_list ap2;
    va_copy(ap2, ap);
    klee_vfmessage(stderr, pfx, msg, ap2);
    va_end(ap2);
  }

  klee_vfmessage(pfx ? klee_warning_file : klee_message_file, pfx, msg, ap);
}

void klee::klee_message(const char *msg, ...) {
  va_list ap;
  va_start(ap, msg);
  klee_vmessage(NULL, false, msg, ap);
  va_end(ap);
}

/* Message to be written only to file */
void klee::klee_message_to_file(const char *msg, ...) {
  va_list ap;
  va_start(ap, msg);
  klee_vmessage(NULL, true, msg, ap);
  va_end(ap);
}

void klee::klee_error(const char *msg, ...) {
  va_list ap;
  va_start(ap, msg);
  klee_vmessage(errorPrefix, false, msg, ap);
  va_end(ap);
  exit(1);
}

void klee::klee_warning(const char *msg, ...) {
  va_list ap;
  va_start(ap, msg);
  klee_vmessage(warningPrefix, false, msg, ap);
  va_end(ap);
}

/* Prints a warning once per message. */
void klee::klee_warning_once(const void *id, const char *msg, ...) {
  static std::set<std::pair<const void *, const char *> > keys;
  std::pair<const void *, const char *> key;

  /* "calling external" messages contain the actual arguments with
     which we called the external function, so we need to ignore them
     when computing the key. */
  if (strncmp(msg, "calling external", strlen("calling external")) != 0)
    key = std::make_pair(id, msg);
  else
    key = std::make_pair(id, "calling external");

  if (!keys.count(key)) {
    keys.insert(key);
    va_list ap;
    va_start(ap, msg);
    klee_vmessage(warningOncePrefix, false, msg, ap);
    va_end(ap);
  }
}
