#!/usr/bin/env bash

# This install script is intended to download and install the latest available
# release of the Ecsact SDK.
#
# You can install using this script:
#   curl -L https://ecsact.dev/install.sh | sh
# Or directly from GitHub:
#   curl https://raw.githubusercontent.com/ecsact-dev/ecsact_sdk/main/install.sh | sh

# Installer script inspired by: https://github.com/wasmerio/wasmer-install/blob/master/install.sh

RELEASES_URL="https://github.com/ecsact-dev/ecsact_sdk/releases"

reset="\033[0m"
red="\033[31m"
green="\033[32m"
yellow="\033[33m"
white="\033[37m"
bold="\e[1m"
dim="\e[2m"

init_env_vars() {
	ARCH=$(uname -m)
	case $ARCH in
		amd64)    ARCH="amd64"    ;;
		x86_64)   ARCH="amd64"    ;;
		*)        echo "${ARCH} systems are not yet supported. Please open an issue on the project if you would like to use Ecsact in your project: https://github.com/ecsact-dev/ecsact_sdk"; exit 1  ;;
	esac

	OS=$(uname | tr '[:upper:]' '[:lower:]')
}

download_json() {
	url="$2"

	# echo "Fetching $url.."
	if test -x "$(command -v curl)"; then
		response=$(curl -s -L -w 'HTTPSTATUS:%{http_code}' -H 'Accept: application/json' "$url")
		body=$(echo "$response" | sed -e 's/HTTPSTATUS\:.*//g')
		code=$(echo "$response" | tr -d '\n' | sed -e 's/.*HTTPSTATUS://')
	elif test -x "$(command -v wget)"; then
		temp=$(mktemp)
		body=$(wget -q --header='Accept: application/json' -O - --server-response "$url" 2>"$temp")
		code=$(awk '/^HTTP/{print $2}' <"$temp" | tail -1)
		rm "$temp"
	else
		printf "Neither curl nor wget was available to perform http requests"
		return 1
	fi
	if [ "$code" != 200 ]; then
		echo "File download failed with code $code"
		return 1
	fi

	eval "$1='$body'"
	return 0
}

download_file() {
	url="$1"
	destination="$2"

	if test -x "$(command -v curl)"; then
		code=$(curl -s -w '%{http_code}' -L "$url" -o "$destination")
	elif test -x "$(command -v wget)"; then
		code=$(wget --quiet -O "$destination" --server-response "$url" 2>&1 | awk '/^  HTTP/{print $2}' | tail -1)
	else
		printf "Neither curl nor wget was available to perform http requests."
		return 1
	fi

	if [ "$code" = 404 ]; then
		printf "\nNot Found - $url\n"
		printf "  Your platform may not yet be supported ($OS-$ARCH).$reset\n"
		printf "  Please open an issue on the project if you would like to use Ecsact in your project: https://github.com/ecsact-dev/ecsact_sdk\n\n"
		return 1
	elif [ "$code" != 200 ]; then
		printf "File download failed with code $code"
		return 1
	fi
	return 0
}

install_ecsact_sdk_deb_package() {
	printf "${dim}Fetching latest release...${reset}\n"
	download_json LATEST_RELEASE "$RELEASES_URL/latest" || return 1

	RELEASE_TAG=$(echo "${LATEST_RELEASE}" | tr -s '\n' ' ' | sed 's/.*"tag_name":"//' | sed 's/".*//')

	printf "Download and install Ecsact SDK ${green}$RELEASE_TAG?${reset}"
	confirm_or_exit || return 1

	DEB_PACKAGE="ecsact_sdk_${RELEASE_TAG}_${ARCH}.deb"
	DEB_PACKAGE_URL="$RELEASES_URL/download/$RELEASE_TAG/$DEB_PACKAGE"
	DOWNLOAD_FILE=$(mktemp -t ecsact_sdk_$RELEASE_TAG.XXXXXXXXXX.deb)

	download_file "$DEB_PACKAGE_URL" "$DOWNLOAD_FILE" || return 1

	printf "Installing Ecsact SDK with dpkg...\n"
	sudo dpkg -i $DOWNLOAD_FILE
	rm -f $DOWNLOAD_FILE
}

confirm_or_exit() {
	if ! [ -t 0 ]; then
		printf "$1 [y/N] ${green}Yes ${dim}(non-interactive)${reset}\n"
		return 0
	fi

	if [ -n "$BASH_VERSION" ]; then
		read -p "$1 [y/N] " -n 1 -r
		if [[ ! $REPLY =~ ^[Yy]$ ]]; then
			printf "${red}\bNo${reset}\n"
			return 1
		fi

		printf "${green}\bYes${reset}\n"

		return 0
	fi

	read -p "$1 [y/N] " yn
	case $yn in
		[Yy]*) printf "${green}Yes${reset}\n" break;                      ;;
		[Nn]*) printf "${red}No${reset}\n"; return 1                      ;;
		*)     printf "\n${red}Installation aborted${reset}\n"; return 1  ;;
	esac

	return 0
}

install_ecsact_sdk() {
	cyan="${reset}\033[36;1m"
	oran="${reset}\e[38;5;214m"

	printf "${reset}Welcome to the Ecsact SDK bash installer!${reset}\n"

	printf "
${cyan}         /##########\
${cyan}     ####  #/     \##\
${cyan}    ######         \##\
${cyan}     ####  ##\      \#        ${oran}|####
${cyan}           \##\        ####  ${oran} #########
${oran}      ############### ${cyan}###### ${oran}##############
${cyan}                       ####  ${oran} #########
${cyan}                   /#         ${oran}|####
${cyan}         ####     /####/
${cyan}        ###### ####/
${cyan}         ####
${reset}
"
	if test -x "$(command -v dpkg)"; then
		install_ecsact_sdk_deb_package
	else
		echo "Only deb package supporting linux distributions can use this install script."
		exit 1
	fi
}

init_env_vars
install_ecsact_sdk

