#!/bin/sh

export VERSION=$(cat version.txt)
export SHORT_HASH=$(git rev-parse --short HEAD)
export REPO_TAG=$VERSION-$SHORT_HASH
export EROSLAB_HOST=$(echo "${CI_REPOSITORY_URL}" | sed -e 's|https\?://gitlab-ci-token:.*@||g' | sed -e 's|/.*||g')
export SSH_HOST=$(echo "${CI_REPOSITORY_URL}" | sed -e 's|https\?://gitlab-ci-token:.*@|git@|g' | sed -e 's|\/lsrd|:lsrd|g')

echo "setting up SSH access to $EROSLAB_HOST"
eval $(ssh-agent -s)
echo "$SSH_RELEASE_KEY" | tr -d '\r' | ssh-add -
mkdir -p ~/.ssh
chmod 700 ~/.ssh

ssh-keyscan "$EROSLAB_HOST" >> ~/.ssh/known_hosts
chmod 644 ~/.ssh/known_hosts
ssh -T git@"${EROSLAB_HOST}"

echo "running git clone, fetch, tag, push from /tmp/source"
cd /tmp && git clone ${SSH_HOST} ./source && cd source/
git remote set-url origin "${SSH_HOST}"
git fetch origin
git checkout master
git tag $REPO_TAG
git push origin $REPO_TAG

echo "cleaning up /tmp/source"
cd / && rm -rf /tmp/source
