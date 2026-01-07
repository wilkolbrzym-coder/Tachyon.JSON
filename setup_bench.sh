#!/bin/bash
set -e

# Create include directories
mkdir -p include/nlohmann
mkdir -p include/glaze_temp

# 1. Download Nlohmann JSON
echo "Downloading Nlohmann JSON..."
wget -q -O include/nlohmann/json.hpp https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp

# 2. Download Simdjson
echo "Downloading Simdjson..."
wget -q -O include/simdjson.h https://github.com/simdjson/simdjson/releases/download/v3.6.0/simdjson.h
wget -q -O include/simdjson.cpp https://github.com/simdjson/simdjson/releases/download/v3.6.0/simdjson.cpp

# 3. Download Glaze
echo "Downloading Glaze..."
wget -q -O glaze.zip https://github.com/stephenberry/glaze/archive/refs/tags/v6.5.1.zip
unzip -q glaze.zip -d include/glaze_temp
# Move the content of include/glaze from the zip to our include/glaze
# The zip structure is typically glaze-6.5.1/include/glaze
mkdir -p include/glaze
cp -r include/glaze_temp/glaze-6.5.1/include/glaze/* include/glaze/
rm -rf include/glaze_temp glaze.zip

echo "Dependencies set up in include/"
ls -F include/
