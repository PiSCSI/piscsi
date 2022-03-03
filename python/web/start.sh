#!/usr/bin/env bash
set -e
# set -x # Uncomment to Debug

cd "$(dirname "$0")"
# verify packages installed
ERROR=0
if ! command -v genisoimage &> /dev/null ; then
    echo "genisoimage could not be found"
    echo "Run 'sudo apt install genisoimage' to fix."
    ERROR=1
fi
if ! command -v python3 &> /dev/null ; then
    echo "python3 could not be found"
    echo "Run 'sudo apt install python3' to fix."
    ERROR=1
fi
if ! python3 -m venv --help &> /dev/null ; then
    echo "venv could not be found"
    echo "Run 'sudo apt install python3-venv' to fix."
    ERROR=1
fi
if ! command -v unzip &> /dev/null ; then
    echo "unzip could not be found"
    echo "Run 'sudo apt install unzip' to fix."
    ERROR=1
fi
if [ $ERROR = 1 ] ; then
    echo
    echo "Fix errors and re-run ./start.sh"
    exit 1
fi

# Test for two known broken venv states
if test -e venv; then
    GOOD_VENV=true
    if ! test -e venv/bin/activate; then
        GOOD_VENV=false
    else
        source venv/bin/activate
        pip3 list 1> /dev/null
        test $? -eq 1 && GOOD_VENV=false
    fi
    if ! "$GOOD_VENV"; then
        echo "Deleting bad python venv"
        sudo rm -rf venv
    fi
fi

# Create the venv if it doesn't exist
if ! test -e venv; then
    echo "Creating python venv for web server"
    python3 -m venv venv
    echo "Activating venv"
    source venv/bin/activate
    echo "Installing requirements.txt"
    pip3 install wheel
    pip3 install -r requirements.txt

    git rev-parse --is-inside-work-tree &> /dev/null
    if [[ $? -eq 0 ]]; then
      git rev-parse HEAD > current
    fi
fi

source venv/bin/activate

# Detect if someone updates the git repo - we need to re-run pip3 install.
set +e

git rev-parse --is-inside-work-tree &> /dev/null
if [[ $? -eq 0 ]]; then
    set -e
    if ! test -e current; then
        git rev-parse > current
    elif [ "$(cat current)" != "$(git rev-parse HEAD)" ]; then
        echo "New version detected, updating libraries from requirements.txt"
        pip3 install -r requirements.txt
        git rev-parse HEAD > current
    fi
else
    echo "Warning: Not running from a valid git repository. Will not be able to update the code."
fi
set -e

pybabel compile -d src/translations

# parse arguments
while [ "$1" != "" ]; do
    PARAM=$(echo "$1" | awk -F= '{print $1}')
    VALUE=$(echo "$1" | awk -F= '{print $2}')
    case $PARAM in
	-p | --port)
	    PORT="--port $VALUE"
	    ;;
	-P | --password)
	    PASSWORD="--password $VALUE"
	    ;;
        *)
            echo "ERROR: unknown parameter \"$PARAM\""
            exit 1
            ;;
    esac
    shift
done

PYTHON_COMMON_PATH=$(dirname $PWD)/common/src
echo "Starting web server for RaSCSI Web Interface..."
export PYTHONPATH=$PWD/src:${PYTHON_COMMON_PATH}
cd src
python3 web.py ${PORT} ${PASSWORD}
