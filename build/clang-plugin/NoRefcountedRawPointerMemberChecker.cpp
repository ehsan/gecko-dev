/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NoRefcountedRawPointerMemberChecker.h"
#include "CustomMatchers.h"

void NoRefcountedRawPointerMemberChecker::registerMatchers(MatchFinder* AstMatcher) {
  AstMatcher->addMatcher(fieldDecl(hasType(pointerType(pointee(isRefCounted()))),
                                   unless(isStrongOrWeakRef())).bind("node"),
      this);
}

void NoRefcountedRawPointerMemberChecker::check(
    const MatchFinder::MatchResult &Result) {
  const FieldDecl *node = Result.Nodes.getNodeAs<FieldDecl>("node");

  if (!getenv("MOZ_BAD_REFS")) {
    return;
  }
  diag(node->getLocStart(), "Raw pointer member %0 points to refcounted class %1",
       DiagnosticIDs::Error) <<
    node << node->getType()->getPointeeType();
  diag(node->getLocStart(), "Please use the appropriate smart pointer class (such as nsCOMPtr, RefPtr)",
       DiagnosticIDs::Note);
}
