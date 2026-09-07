#!/bin/bash
read -p "Commit: " m && git add . && git commit -m "$m" && git pull https://github.com/SchlonnyTech/LowLevelLua.git main --rebase && git push https://github.com/SchlonnyTech/LowLevelLua.git main
