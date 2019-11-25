/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ThirdPartyUtil_h__
#define ThirdPartyUtil_h__

#include "mozIThirdPartyUtil.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/Document.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsEffectiveTLDService.h"
#include "nsString.h"
#include "nsPIDOMWindow.h"

class nsIURI;
class nsPIDOMWindowOuter;

class ThirdPartyUtil final : public mozIThirdPartyUtil {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_MOZITHIRDPARTYUTIL

  nsresult Init();

  static void Startup();
  static ThirdPartyUtil* GetInstance();

  nsresult CheckWindow(mozIDOMWindowProxy* aWindow, nsIURI* aURI,
                       CanonicalNameConsiderations aConsiderations,
                       bool* aResult, bool* aCanonicalHostNameWasMaterial);
  nsresult CheckChannel(nsIChannel* aChannel, nsIURI* aURI,
                        CanonicalNameConsiderations aConsiderations,
                        bool* aResult, bool* aCanonicalHostNameWasMaterial);

  bool URIMatchesCanonicalHostName(nsIURI* aURI, const nsACString& aName);
  nsresult IsThirdPartyInternal(const nsACString& aFirstDomain,
                                nsIURI* aSecondURI, bool* aResult);

 private:
  ~ThirdPartyUtil();

  bool IsThirdPartyInternal(const nsACString& aFirstDomain,
                            const nsACString& aSecondDomain) {
    // Check strict equality.
    return aFirstDomain != aSecondDomain;
  }

  nsCString GetBaseDomainFromWindow(nsPIDOMWindowOuter* aWindow);

  bool PrincipalMatchesCanonicalHostName(nsIPrincipal* aPrincipal,
                                         const nsACString& aName);

  nsresult GetCanonicalHostNameFromWindow(mozIDOMWindowProxy* aWindow,
                                          nsACString& aResult,
                                          nsACString* aTopWindowResult);

  RefPtr<nsEffectiveTLDService> mTLDService;
};

#endif
