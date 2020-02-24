// This test ensures that the URL decoration annotations service works as
// expected, and also we successfully downgrade document.referrer to the
// eTLD+1 URL when tracking identifiers controlled by this service are
// present in the referrer URI.

/* import-globals-from antitracking_head.js */

"use strict";

AntiTracking._createTask({
  name:
    "Test that cookies are allowed for a tracker in the third-party context when the top-level page is loaded from a host name that resolves to a CNAME from the same eTLD+1 as the tracker",
  cookieBehavior: BEHAVIOR_REJECT_TRACKER,
  blockingByContentBlockingRTUI: false,
  allowList: false,
  callback: async _ => {
    document.cookie = "name=value";
    ok(document.cookie != "", "Nothing is blocked");
  },
  extraPrefs: [
    ["privacy.thirdparty.consider_canonical_hostname", false],
    ["privacy.thirdparty.consider_top_canonical_hostname", true],
  ],
  expectedBlockingNotifications: 0,
  runInPrivateWindow: false,
  iframeSandbox: null,
  accessRemoval: null,
  callbackAfterRemoval: null,
  // topPage is set to foo.tld
  topPage: TEST_TOP_PAGE,
  // canonicalHostName is set to bar.tld
  canonicalHostName: TEST_3RD_PARTY_DOMAIN_SUBDOMAIN,
});

add_task(_ => {
  let cookies = Services.cookies.getCookiesFromHost(
    new URL(TEST_3RD_PARTY_DOMAIN).hostname,
    { firstPartyDomain: new URL(TEST_TOP_PAGE).hostname }
  );
  is(cookies.length, 1, "We should only find one item");
  for (let cookie of cookies) {
    is(cookie.name, "name", "The expected cookie name should be found");
    is(cookie.value, "value", "The expected cookie value should be found");
  }
});

add_task(async _ => {
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
      resolve()
    );
  });
});
