#!/usr/bin/env bash


git log > ChangeLog
touch NEWS
touch README

aclocal
automake --add-missing
automake -f
autoconf -f

./configure $*
