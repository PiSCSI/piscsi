#!/usr/bin/env bash
set -e

cd "$(dirname "$0")"

# Create the venv if it doesn't exist
if ! test -e venv; then
    echo "Creating python venv for PiSCSI-Web development"
    python3 -m venv venv
    echo "Activating venv"
    source venv/bin/activate
    echo "Installing requirements-dev.txt"
    pip3 install wheel
    pip3 install -r requirements.txt
fi

source venv/bin/activate

pybabel extract -F babel.cfg -o messages.pot src
pybabel update -i messages.pot -d src/translations

echo
echo "Translation stats:"
find . -name \*.po -print -execdir msgfmt --statistics messages.po \;
