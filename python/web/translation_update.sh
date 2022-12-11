#!/usr/bin/env bash
set -e

cd "$(dirname "$0")"
# Check for the existence of a python venv in the current dir
if ! test -e venv; then
    echo "No python venv detected. Please run start.sh first."
    exit 1
fi

source venv/bin/activate

pybabel extract -F babel.cfg -o messages.pot .
pybabel update -i messages.pot -d src/translations

echo
echo "Translation stats:"
find . -name \*.po -print -execdir msgfmt --statistics messages.po \;
