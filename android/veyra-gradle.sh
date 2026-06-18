#!/usr/bin/env bash
# Wrapper that runs the project's Gradle with the correct JDK/SDK, since this
# repo has no gradle wrapper (gradlew) committed. Usage from android/:
#   ./veyra-gradle.sh assembleDebug
#   ./veyra-gradle.sh installDebug
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/../scripts/android-env.sh"
exec "$GRADLE_HOME/bin/gradle" --project-dir "$SCRIPT_DIR" "$@"
