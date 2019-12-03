/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/

"use strict";

add_test(async _ => {
  Services.prefs.setBoolPref("browser.safebrowsing.passwords.enabled", true);

  let classifier = Cc["@mozilla.org/url-classifier/dbservice;1"].getService(
    Ci.nsIURIClassifier
  );
  ok(!!classifier, "We have the URI-Classifier");

  var tests = [
    { name: "a", expectedResult: false },
    { name: "tracking-annotation", expectedResult: true },
    { name: "tracking-protection", expectedResult: true },
    { name: "login-reputation", expectedResult: true },
  ];

  tests.forEach(test => {
    let feature;
    try {
      feature = classifier.getFeatureByName(test.name);
    } catch (e) {}

    equal(
      !!feature,
      test.expectedResult,
      "Exceptected result for: " + test.name
    );
    if (feature) {
      equal(feature.name, test.name, "Feature name matches");
    }
  });

  let uri = Services.io.newURI("https://example.com");
  let prin = Services.scriptSecurityManager.createContentPrincipal(uri, {});
  let uri2 = Services.io.newURI("https://otherhost.com");
  let prin2 = Services.scriptSecurityManager.createContentPrincipal(uri2, {});

  let feature = classifier.getFeatureByName("tracking-protection");

  let results = await new Promise(resolve => {
    classifier.asyncClassifyLocalWithFeatures(
      uri,
      [feature],
      Ci.nsIUrlClassifierFeature.blacklist,
      "",
      r => {
        resolve(r);
      }
    );
  });
  equal(results.length, 0, "No tracker");

  Services.prefs.setCharPref(
    "urlclassifier.trackingTable.testEntries",
    "example.com"
  );

  feature = classifier.getFeatureByName("tracking-protection");

  results = await new Promise(resolve => {
    classifier.asyncClassifyLocalWithFeatures(
      uri,
      [feature],
      Ci.nsIUrlClassifierFeature.blacklist,
      "",
      r => {
        resolve(r);
      }
    );
  });
  equal(results.length, 1, "Tracker");
  let result = results[0];
  equal(result.feature.name, "tracking-protection", "Correct feature");
  equal(result.list, "tracking-blacklist-pref", "Correct list");

  results = await new Promise(resolve => {
    classifier.asyncClassifyLocalWithFeatures(
      uri2,
      [feature],
      Ci.nsIUrlClassifierFeature.blacklist,
      uri.asciiHost,
      r => {
        resolve(r);
      }
    );
  });
  equal(results.length, 1, "Tracker");
  result = results[0];
  equal(result.feature.name, "tracking-protection", "Correct feature");
  equal(result.list, "tracking-blacklist-pref", "Correct list");

  result = classifier.classify(prin, null, "", function(
    errorCode,
    list,
    provider,
    fullHash
  ) {
    ok(true, "Tracker should be found");
  });
  ok(result, "Classification should succeed");

  result = classifier.classify(prin2, null, uri.asciiHost, function(
    errorCode,
    list,
    provider,
    fullHash
  ) {
    ok(true, "Tracker should be found");
  });
  ok(result, "Classification should succeed");

  Services.prefs.clearUserPref("browser.safebrowsing.password.enabled");
  run_next_test();
});
