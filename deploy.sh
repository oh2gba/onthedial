#!/usr/bin/env bash
# Upload ./site to the project web page over FTP with explicit TLS.
# Credentials come from ./.env (git-ignored):
#   FTP_HOST, FTP_USER, FTP_PASS, SITE_HOST and optionally FTP_DIR.
set -euo pipefail
cd "$(dirname "$0")"

if [ ! -f .env ]; then
  echo ".env with FTP_HOST/FTP_USER/FTP_PASS is missing" >&2
  exit 1
fi
set -a
# shellcheck disable=SC1091
. ./.env
set +a
: "${FTP_HOST:?}" "${FTP_USER:?}" "${FTP_PASS:?}"
FTP_DIR="${FTP_DIR:-}"
FTP_DIR="${FTP_DIR%/}"

# curl reads the credentials from a temporary netrc file, so the password
# never appears in the process list.
NETRC="$(mktemp)"
trap 'rm -f "$NETRC"' EXIT
printf 'machine %s login %s password %s\n' "$FTP_HOST" "$FTP_USER" "$FTP_PASS" > "$NETRC"

cd site
find . -type f | sort | while read -r f; do
  rel="${f#./}"
  echo "  $rel"
  curl --silent --show-error --ssl-reqd --netrc-file "$NETRC" --ftp-create-dirs \
       -T "$f" "ftp://$FTP_HOST/$FTP_DIR${FTP_DIR:+/}$rel"
done
echo "Deployed to https://${SITE_HOST:-$FTP_HOST}/"
