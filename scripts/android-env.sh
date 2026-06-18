#!/usr/bin/env bash
# Veyra Android build environment. Source this (or rely on the lines appended
# to ~/.bashrc / ~/.zshrc) before building the Android app.
#
#   source scripts/android-env.sh
#   cd android && ./veyra-gradle.sh assembleDebug
#
# Toolchain locations on this machine (everything under /mnt/zboth).

export ANDROID_HOME="/mnt/zboth/android-sdk"
export ANDROID_SDK_ROOT="$ANDROID_HOME"
export ANDROID_NDK_HOME="$ANDROID_HOME/ndk/27.3.13750724"
export ANDROID_NDK_ROOT="$ANDROID_NDK_HOME"

# AGP 8.2.2 needs JDK 17+; JDK 21 is the newest the toolchain accepts here
# (JDK 25 is too new for the Android Gradle Plugin).
export JAVA_HOME="/usr/lib/jvm/java-21-openjdk-amd64"

export GRADLE_HOME="/mnt/zboth/gradle810/gradle-8.10.2"

export PATH="$JAVA_HOME/bin:$GRADLE_HOME/bin:$ANDROID_HOME/platform-tools:$ANDROID_HOME/cmdline-tools/latest/bin:$PATH"
