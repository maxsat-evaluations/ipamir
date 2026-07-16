#!/usr/bin/env bash

FILES=$(mktemp)

git ls-tree -r main --name-only | grep -v '^flake' | grep -v '^\.' | grep -v 'inputs\.zip$' >> $FILES

for app in app/ipamir*; do
  find $app/inputs -type f >> $FILES
done

cat $FILES | zip -r incremental.zip -@
