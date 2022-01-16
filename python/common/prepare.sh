#!/usr/bin/env bash
set -e
pybabel extract -F babel.cfg src/rascsi -o src/rascsi/translations/messages.pot
#pybabel init -i src/rascsi/translations/messages.pot -d src/rascsi/translations -l de
pybabel compile -d src/rascsi/translations
#pybabel compile -d src/translations
