#!/usr/bin/env bash
set -x

mkdir -p .delta
#rm -rf .delta/*
tags=$(git ls-remote -t --symref | grep -v '\^' | cut -f 2 | sed s@refs/tags/@@g)
# exract 5 history versions
versions=$(echo "$tags" | tail -6 | head -5 | xargs | tr ' ' ',')
curl -f -L -o .delta/Clipy.app-#1.zip "https://github.com/ian4hu/Clipy/releases/download/{$versions}/Clipy.app.zip"