#!/bin/bash

#add remote branch
#git remote add yap git://yap.git.sourceforge.net/gitroot/yap/yap-6.3
#git fetch yap
#git checkout -b yap yap/master

# switch to yap branch
git checkout yap
# pull changes
git pull

# switch over to master
git checkout master

#merge yap
git merge yap

## submodules

# git submodule init
# git submodule update

# checkout master and update
# git submodule foreach "(git checkout master; git pull)"

# update submodules
git submodule foreach git pull

#push merged changes
git push
