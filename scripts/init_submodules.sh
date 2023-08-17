#bin/bash

if git submodule status | grep --quiet '^-'; then
    git submodule init
    git submodule update
    echo "Initialized submodules"
else
    echo "All submodule already up-to-date."
fi