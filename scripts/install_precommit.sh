#!/bin/bash

SCRIPTPATH="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"

cd $SCRIPTPATH && cd .. #Make sure that the working directory is in the repo root folder

if [ -f ".git/hooks/pre-commit" ]; then
  mv .git/hooks/pre-commit .git/hooks/pre-commit.backup
  echo "Moved old pre-commit hook to .git/hooks/pre-commit.backup"
fi

command -v pre-commit >/dev/null 2>&1 || pip3 install --user pre-commit
pre-commit install

mv .git/hooks/pre-commit .git/hooks/pre-commit-internal
cp scripts/pre-commit.template .git/hooks/pre-commit