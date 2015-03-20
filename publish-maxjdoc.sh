#!/bin/sh

# generate maxjdoc
ant maxjdoc

# clone GitHub pages branch
git clone --branch=gh-pages git+ssh://git@github.com/maxeler/maxpower.git gh-pages
cd gh-pages

# replace doc directory
git rm -rf maxjdoc
cp -r ../maxjdoc maxjdoc

# commit and push to GitHub
git add -f maxjdoc
git commit -m "Updated MaxJDoc"
git push origin

# clean up
cd ..
rm -rf gh-pages
