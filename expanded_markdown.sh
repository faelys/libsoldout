#!/bin/sh

expand -t 4 "$@" | $(dirname $0)/markdown
