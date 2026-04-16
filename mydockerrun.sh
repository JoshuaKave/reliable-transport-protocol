#!/bin/bash

docker run --rm -it -v "$(pwd)":/project -w /project project-1a "$@"
