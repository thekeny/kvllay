#!/usr/bin/env bash
set -e

# ==============================================================================
# kvllay - Release Automation Script
# Creates annotated Git tags, handles uncommitted changes, and pushes to remote
# ==============================================================================

# ANSI Color Codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m' # No Color

echo -e "${BOLD}${CYAN}"
echo "=================================================="
echo "          kvllay Release Manager 🚀               "
echo "=================================================="
echo -e "${NC}"

# 0. Help option
if [ "$1" = "-h" ] || [ "$1" = "--help" ]; then
    echo -e "Usage: ./scripts/release.sh [VERSION] or make release [VERSION=vX.Y.Z]"
    echo -e "Examples:"
    echo -e "  ./scripts/release.sh              # Interactive mode with prompts"
    echo -e "  ./scripts/release.sh 1.0.0        # Automatically creates tag v1.0.0"
    echo -e "  ./scripts/release.sh v1.0.0-rc.1  # Creates tag v1.0.0-rc.1"
    echo -e "  make release VERSION=1.0.0        # Via makefile"
    exit 0
fi

# Ensure running inside a git repository
if ! git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    echo -e "${RED}[!] Error: Current directory is not a Git repository.${NC}"
    exit 1
fi

CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
REMOTE="origin"

echo -e "Current branch: ${BOLD}${GREEN}${CURRENT_BRANCH}${NC}"

# 1. Prompt for version / tag
TAG_INPUT="$1"
if [ -z "$TAG_INPUT" ]; then
    CURRENT_CONST_VER=$(grep -oP 'VERSION = "\K[^"]+' include/kvllay/constants.hpp 2>/dev/null || echo "")
    if [ -n "$CURRENT_CONST_VER" ]; then
        echo -e "Version detected in constants.hpp: ${YELLOW}${CURRENT_CONST_VER}${NC}"
    fi

    echo -ne "${BOLD}Enter release version/tag (e.g. 1.0.0 or v1.0.0-rc.1): ${NC}"
    read -r TAG_INPUT
fi

if [ -z "$TAG_INPUT" ]; then
    echo -e "${RED}[!] Error: Release version cannot be empty.${NC}"
    exit 1
fi

# Auto-prepend 'v' if missing (required by GitHub Actions: v*)
if [[ ! "$TAG_INPUT" =~ ^v ]]; then
    TAG="v${TAG_INPUT}"
else
    TAG="$TAG_INPUT"
fi

echo -e "Target tag: ${BOLD}${GREEN}${TAG}${NC}"

# 2. Check if tag already exists
if git rev-parse "$TAG" >/dev/null 2>&1; then
    echo -e "${RED}[!] Tag '${TAG}' already exists locally!${NC}"
    echo -ne "Delete local tag and overwrite? (y/N): "
    read -r OVERWRITE_TAG
    if [[ "$OVERWRITE_TAG" =~ ^[YyДд]$ ]]; then
        git tag -d "$TAG"
    else
        echo -e "${YELLOW}Operation aborted.${NC}"
        exit 1
    fi
fi

# 3. Prompt for release message
echo -ne "${BOLD}Enter release description (press Enter for 'Release ${TAG}'): ${NC}"
read -r RELEASE_MSG

if [ -z "$RELEASE_MSG" ]; then
    RELEASE_MSG="Release ${TAG}"
fi

# 4. Check for uncommitted changes
if ! git diff-index --quiet HEAD --; then
    echo -e "\n${YELLOW}[!] Uncommitted working tree changes detected:${NC}"
    git status -s
    echo ""
    echo -ne "${BOLD}Commit these changes before creating the release? (y/N): ${NC}"
    read -r COMMIT_CHANGES
    if [[ "$COMMIT_CHANGES" =~ ^[YyДд]$ ]]; then
        echo -ne "Enter commit message (press Enter for '${RELEASE_MSG}'): "
        read -r COMMIT_MSG
        if [ -z "$COMMIT_MSG" ]; then
            COMMIT_MSG="${RELEASE_MSG}"
        fi
        git add .
        git commit -m "$COMMIT_MSG"
        echo -e "${GREEN}[✓] Changes committed successfully.${NC}"
    else
        echo -e "${YELLOW}[i] Proceeding without committing working tree changes.${NC}"
    fi
fi

# 5. Summary & Confirmation
echo -e "\n${BOLD}${CYAN}--- Release Summary ---${NC}"
echo -e "  Branch:      ${BOLD}${CURRENT_BRANCH}${NC}"
echo -e "  Tag:         ${BOLD}${GREEN}${TAG}${NC}"
echo -e "  Description: ${BOLD}${RELEASE_MSG}${NC}"
echo -e "  Remote:      ${BOLD}${REMOTE}${NC}"
echo -e "${BOLD}${CYAN}-----------------------${NC}\n"

echo -ne "${BOLD}Push branch '${CURRENT_BRANCH}' and tag '${TAG}' to ${REMOTE}? (y/N): ${NC}"
read -r CONFIRM

if [[ ! "$CONFIRM" =~ ^[YyДд]$ ]]; then
    echo -e "${YELLOW}Release canceled by user.${NC}"
    exit 0
fi

# 6. Create annotated tag
echo -e "\n${BLUE}[*] Creating annotated tag '${TAG}'...${NC}"
git tag -a "$TAG" -m "$RELEASE_MSG"

# 7. Push branch
echo -e "${BLUE}[*] Pushing branch '${CURRENT_BRANCH}' to ${REMOTE}...${NC}"
git push "$REMOTE" "$CURRENT_BRANCH"

# 8. Push tag
echo -e "${BLUE}[*] Pushing tag '${TAG}' to ${REMOTE}...${NC}"
git push "$REMOTE" "$TAG"

# 9. Success output
REMOTE_URL=$(git config --get remote.${REMOTE}.url | sed 's/\.git$//')
REMOTE_URL=$(echo "$REMOTE_URL" | sed -E 's|^git@github\.com:|https://github.com/|')

echo -e "\n${BOLD}${GREEN}=================================================="
echo "           Release Published Successfully! 🎉     "
echo "==================================================${NC}"
echo -e "Tag:            ${BOLD}${TAG}${NC}"
echo -e "GitHub Actions: ${CYAN}${REMOTE_URL}/actions${NC}"
echo -e "GitHub Release: ${CYAN}${REMOTE_URL}/releases${NC}\n"
