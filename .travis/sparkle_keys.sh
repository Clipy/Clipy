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
# replace signature
sed -e 's@sparkle:edSignature=\"[^\"]*\"\ length\=\"[0-9]*\"@'"$sign_out"'@g' -i appcast.xml
# replace version
sed -e 's@sparkle:version=\"[^\"]*\"@sparkle:version="'"$TRAVIS_TAG"'"@g' \
    -e 's@sparkle:shortVersionString==\"[^\"]*\"@sparkle:shortVersionString="'"$TRAVIS_TAG"'"@g' -i appcast.xml
