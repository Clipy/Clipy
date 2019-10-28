#!/usr/bin/env bash
set -x
#security create-keychain -p $MATCH_PASSWORD $MATCH_KEYCHAIN_NAME
#security default-keychain -s $MATCH_KEYCHAIN_NAME
#security unlock-keychain -p $MATCH_PASSWORD $MATCH_KEYCHAIN_NAME
#security set-keychain-settings -t 10 -u $MATCH_KEYCHAIN_NAME

# import ed25519 keys
#security add-generic-password -a 'ed25519' \
#    -s 'https://sparkle-project.org' \
#    -D 'private key' \
#    -l 'Private key for signing Sparkle updates' \
#    -j "$SPARKLE_ED25519_KEY_COMMENT" \
#    -w "$SPARKLE_ED25519_KEY" \
#    -T $PWD/Pods/Sparkle/bin/sign_update \
#    -U

#security set-key-partition-list -S apple-tool:,apple: -s -k $MATCH_PASSWORD $MATCH_KEYCHAIN_NAME

# gen signature
#sign_out=$(Pods/Sparkle/bin/sign_update Clipy.app.zip)
#signature=$(echo $sign_out | sed -e 's@sparkle:edSignature="@@g' -e 's@" length=.*@@g')
#length=$(echo $sign_out | sed -e 's@.*length="@@g' -e 's@"$@@g')
#date=$(date)
#
#cat appcast_tpl.xml | sed \
#    -e 's@__VERSION__@'"$TRAVIS_TAG"'@g' \
#    -e 's@__SIGNATURE__@'"$signature"'@g' \
#    -e 's@__LENGTH__@'"$length"'@g' \
#    -e 's@__TIME__@'"$date"'@g' \
#    > appcast.xml

# gen signature
rm -rf ~/Library/Caches/Sparkle*
rm -rf .tmp/*
mkdir -p .tmp
cp Clipy.app.zip .tmp/
#cp appcast_tpl.xml .tmp/appcast.xml
Pods/Sparkle/bin/generate_appcast -s "$SPARKLE_ED25519_KEY" .tmp/

# edit release note
cat .tmp/appcast.xml | \
    sed -E 's@^( .*)(<pubDate>)@\1<sparkle:releaseNotesLink>https://github.com/ian4hu/Clipy/releases/latest</sparkle:releaseNotesLink>\
\1\2@g' \
    > appcast.xml

#cp .tmp/appcast.xml ./
#rm -rf .tmp