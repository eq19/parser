#!/usr/bin/env bash

git config --global user.name "${{ github.actor }}"
git config --global user.email "${{ github.actor }}@users.noreply.github.com"

git config --global core.autocrlf false
git config --global --add safe.directory "${{ github.workspace }}"       

echo 'LATEST_COMMIT='$(curl -s "https://api.github.com/users/eq19/events/public" | jq ".[0].payload.commits[0].message") > ${GITHUB_ENV}
echo 'REMOTE_REPO=https://${{ github.actor }}:${TOKEN}@github.com/${{ github.repository }}.git' > ${GITHUB_ENV}
