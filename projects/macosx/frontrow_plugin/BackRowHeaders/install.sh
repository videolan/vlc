#!/bin/sh

if test ! -e $PWD/Headers/BREvent.h; then
    echo "Please run this script from BackRowHeaders directory"
    exit 1
else
    sudo ln -sf $PWD/Headers /System/Library/PrivateFrameworks/BackRow.framework
fi
