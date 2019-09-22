#!/usr/bin/env bash

security create-keychain -p $MATCH_PASSWORD $MATCH_KEYCHAIN_NAME
security default-keychain -s $MATCH_KEYCHAIN_NAME
security unlock-keychain -p $MATCH_PASSWORD $MATCH_KEYCHAIN_NAME
security set-keychain-settings -t 3600 -u $MATCH_KEYCHAIN_NAME

security add-generic-password -a 'ed25519' \
    -s 'https://sparkle-project.org' \
    -D 'private key' \
    -l 'Private key for signing Sparkle updates' \
    -j "$SPARKLE_ED25519_KEY_COMMENT" \
    -w "$SPARKLE_ED25519_KEY"

# gen signature
sign_out=$(Pods/Sparkle/bin/sign_update Clipy.app.zip)
signature=$(echo $sign_out | sed -e 's@sparkle:edSignature="@@g' -e 's@" length=.*@@g')
length=$(echo $sign_out | sed -e 's@.*length="@@g' -e 's@"$@@g')
date=$(date)

cat appcast_tpl.xml | sed \
    -e 's@__VERSION__@'"$TRAVIS_TAG"'@g' \
    -e 's@__SIGNATURE__@'"$signature"'@g' \
    -e 's@__LENGTH__@'"$length"'@g' \
    -e 's@__TIME__@'"$date"'@g' \
    > appcast.xml
