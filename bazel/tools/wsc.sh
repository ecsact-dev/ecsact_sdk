#!/usr/bin/env bash

set -e

if [[ -z "${GITHUB_REF}" ]]; then
	echo "STABLE_ECSACT_SDK_VERSION $(git describe --tags --abbrev=0)"
else
	echo "STABLE_ECSACT_SDK_VERSION ${GITHUB_REF##*/}"
fi

