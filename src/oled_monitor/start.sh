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

if ! i2cdetect -y 1 &> /dev/null ; then
    echo "i2cdetect -y 1 did not find a screen."
    exit 2
fi
if ! test -e venv; then
  echo "Creating python venv for OLED Screen"
  python3 -m venv venv
  echo "Activating venv"
  source venv/bin/activate
  echo "Installing requirements.txt"
  pip install wheel
  pip install -r requirements.txt
  git rev-parse HEAD > current
fi

source venv/bin/activate

# Detect if someone updates - we need to re-run pip install.
if ! test -e current; then
  git rev-parse > current
else
  if [ "$(cat current)" != "$(git rev-parse HEAD)" ]; then
      echo "New version detected, updating requirements.txt"
      pip install -r requirements.txt
      git rev-parse HEAD > current
  fi
fi

# parse arguments
while [ "$1" != "" ]; do
    PARAM=$(echo "$1" | awk -F= '{print $1}')
    VALUE=$(echo "$1" | awk -F= '{print $2}')
    case $PARAM in
	-r | --rotation)
	    ROTATION=$VALUE
	    ;;
        *)
            echo "ERROR: unknown parameter \"$PARAM\""
            exit 1
            ;;
    esac
    case $VALUE in
        0 | 180 )
            ;;
        *)
            echo "ERROR: invalid option \"$VALUE\""
	    exit 1
            ;;
    esac
    shift
done

echo "Starting OLED Screen with $ROTATION degrees rotation..."
python3 rascsi_oled_monitor.py "${ROTATION}"
