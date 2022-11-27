#!/bin/sh
/Applications/Aseprite.app/Contents/MacOS/aseprite --batch "$1" --script-param output="$2" --script ~/Library/Application\ Support/Aseprite/extensions/playdate-ani-exporter/main.lua
