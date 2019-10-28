#!/usr/bin/env bash
set -x

mkdir -p .delta
#rm -rf .delta/*
# exract 5 history versions
versions=$(git tag -l --sort="v:refname" | tail -6 | head -5 | xargs | tr ' ' ',')
curl --retry 3 --connect-timeout 20 -m 300 -f -L -o .delta/Clipy.app-#1.zip "https://github.com/ian4hu/Clipy/releases/download/{$versions}/Clipy.app.zip"