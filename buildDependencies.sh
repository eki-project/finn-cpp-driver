#!/bin/bash

#Setup
git submodule update --init --recursive
if [ ! -d "deps" ]; then
  mkdir -p "deps"
else
  rm -rf "deps"/*
fi
cd external/boost

#Build minimal version of boost needed for renaming
./bootstrap.sh || exit 1  #bootstrap b2 build system
./b2 headers || exit 1 
./b2 tools/bcp tools/boost_install || exit 1 


#Copy and rename boost
if [ ! -d "../../deps/finn_boost" ]; then
  mkdir -p "../../deps/finn_boost"
else
  rm -rf "../../deps/finn_boost"/*
fi

./dist/bin/bcp --namespace=finnBoost log build boost_install config predef program_options thread filesystem circular_buffer ../../deps/finn_boost/ || exit 1 
cp boostcpp.jam ../../deps/finn_boost/
cp boost-build.jam ../../deps/finn_boost/
cp project-config.jam ../../deps/finn_boost/
cp bootstrap.sh ../../deps/finn_boost/

#Cleanup of submodule
./b2 --clean-all
git clean -f -x -d
git submodule foreach --recursive git clean -xfd
git reset --hard
git submodule foreach --recursive git reset --hard
cd ../../deps/finn_boost

#Build new finnBoost Version
./bootstrap.sh || exit 1  #bootstrap b2 build system for finnBoost
./b2 --without-python || exit 1 
mv stage/lib/boost-* stage/lib/boost 2>/dev/null || true

echo "Dependecies successfully build!"
echo "You can now build the finn-cpp-driver."


