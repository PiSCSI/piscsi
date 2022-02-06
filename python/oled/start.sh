#!/usr/bin/env bash
set -e
# set -x # Uncomment to Debug

cd "$(dirname "$0")"
# verify packages installed
ERROR=0
if ! command -v dpkg -l i2c-tools &> /dev/null ; then
    echo "i2c-tools could not be found"
    echo "Run 'sudo apt install i2c-tools' to fix."
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
# Dep to build Pillow
if ! dpkg -l python3-dev &> /dev/null; then
    echo "python3-dev could not be found"
    echo "Run 'sudo apt install python3-dev' to fix."
    ERROR=1
fi
if ! dpkg -l libjpeg-dev &> /dev/null; then
    echo "libjpeg-dev could not be found"
    echo "Run 'sudo apt install libjpeg-dev' to fix."
    ERROR=1
fi
if ! dpkg -l libpng-dev &> /dev/null; then
    echo "libpng-dev could not be found"
    echo "Run 'sudo apt install libpng-dev' to fix."
    ERROR=1
fi
if ! dpkg -l libopenjp2-7-dev &> /dev/null; then
    echo "libopenjp2-7-dev could not be found"
    echo "Run 'sudo apt install libopenjp2-7-dev' to fix."
    ERROR=1
fi
if [ $ERROR = 1 ] ; then
  echo
  echo "Fix errors and re-run ./start.sh"
  exit 1
fi

if pgrep -f "python3 src/rascsi_oled_monitor.py" &> /dev/null; then
    echo "Detected active rascsi_oled_monitor.py process"
    echo "Terminating before launching a new one."
    sudo pkill -f "python3 src/rascsi_oled_monitor.py"
fi

if ! i2cdetect -y 1 &> /dev/null ; then
    echo "i2cdetect -y 1 did not find a screen."
    exit 2
fi

# Compiler flags needed for gcc v10 and up
if [[ `gcc --version | awk '/gcc/' | awk -F ' ' '{print $3}' | awk -F '.' '{print $1}'` -ge 10 ]]; then
    COMPILER_FLAGS="-fcommon"
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
    echo "Creating python venv for OLED Screen"
    python3 -m venv venv
    echo "Activating venv"
    source venv/bin/activate
    echo "Installing requirements.txt"
    pip3 install wheel
    CFLAGS="$COMPILER_FLAGS" pip3 install -r requirements.txt

    set +e
    git rev-parse --is-inside-work-tree &> /dev/null
    if [[ $? -eq 0 ]]; then
      git rev-parse HEAD > current
    fi
    set -e
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
        CFLAGS="$COMPILER_FLAGS" pip3 install -r requirements.txt
        git rev-parse HEAD > current
    fi
else
    echo "Warning: Not running from a valid git repository. Will not be able to update the code."
fi
set -e

# parse arguments
while [ "$1" != "" ]; do
    PARAM=$(echo "$1" | awk -F= '{print $1}')
    VALUE=$(echo "$1" | awk -F= '{print $2}')
    case $PARAM in
	-r | --rotation)
	    ROTATION="--rotation $VALUE"
	    ;;
	-h | --height)
	    HEIGHT="--height $VALUE"
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

echo "Starting OLED Screen..."
if [ -z ${ROTATION+x} ]; then
    echo "No screen rotation parameter given; falling back to the default."
else
    echo "Starting with parameter $ROTATION"
fi
if [ -z ${HEIGHT+x} ]; then
    echo "No screen height parameter given; falling back to the default."
else
    echo "Starting with parameter $HEIGHT"
fi

PYTHON_COMMON_PATH=$(dirname $PWD)/common/src
export PYTHONPATH=$PWD/src:${PYTHON_COMMON_PATH}
python3 src/rascsi_oled_monitor.py ${ROTATION} ${HEIGHT} ${PASSWORD}
