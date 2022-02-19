#!/usr/bin/env bash
set -e
# set -x # Uncomment to Debug

PI_MODEL=$(/usr/bin/tr -d '\0' < /proc/device-tree/model)

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

if pgrep -f "python3 src/main.py" &> /dev/null; then
    echo "Detected active rascsi control board service"
    echo "Terminating before launching a new one."
    sudo pkill -f "python3 src/main.py"
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
    ! test -e venv/bin/activate && GOOD_VENV=false
    pip3 list 1> /dev/null
    test $? -eq 1 && GOOD_VENV=false
    if ! "$GOOD_VENV"; then
        echo "Deleting bad python venv"
        sudo rm -rf venv
    fi
fi

# Create the venv if it doesn't exist
if ! test -e venv; then
    echo "Creating python venv for RaSCSI control board service"
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

export PYTHONPATH=${PWD}/src:${PWD}/../common/src

echo "Starting RaSCSI control board service..."

if [[ ${PI_MODEL} =~ "Raspberry Pi 4" ]]; then
  #Model Raspberry Pi 4 Model B Rev 1.4
  echo "Detected: Raspberry Pi 4"
  sudo /usr/sbin/rmmod i2c_bcm2835
  sudo /usr/sbin/modprobe i2c_bcm2835 baudrate=1000000
  python3 src/main.py "$@" --transitions 1
elif [[ ${PI_MODEL} =~ "Raspberry Pi 3" ]] || [[ ${PI_MODEL} =~ "Raspberry Pi Zero 2" ]]; then
  #Model Raspberry Pi Zero 2 W Rev 1.0
  #Model Raspberry Pi 3 Model B Rev 1.2
  echo "Detected: Raspberry Pi 3 or Zero 2"
  sudo /usr/sbin/rmmod i2c_bcm2835
  sudo /usr/sbin/modprobe i2c_bcm2835 baudrate=400000
  python3 src/main.py --transitions 1
else
  #Model Raspberry Pi Zero W Rev 1.1
  #Model Raspberry Pi Zero Rev 1.3
  echo "Detected: Raspberry Pi Zero, Zero W, Zero WH, ..."
  sudo /usr/sbin/rmmod i2c_bcm2835
  sudo /usr/sbin/modprobe i2c_bcm2835 baudrate=100000
  python3 src/main.py --transitions 0
fi
