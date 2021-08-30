#!/usr/bin/env bash
set -e
# set -x # Uncomment to Debug

# parse arguments
while [ "$1" != "" ]; do
    PARAM=`echo $1 | awk -F= '{print $1}'`
    VALUE=`echo $1 | awk -F= '{print $2}'`
    case $PARAM in
        -e | --exclude_id)
	    EXCLUDE_ID=$VALUE
	    ;;
	*)
	    echo "ERROR: unknown parameter \"$PARAM\""
	    exit 1
	    ;;
    esac
    shift
done

cd $(dirname $0)
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

if ! test -e venv; then
  echo "Creating python venv for web server"
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

echo "Starting web server..."
if [ "$EXCLUDE_ID" ]; then
  echo "Reserving SCSI ID" $EXCLUDE_ID
  python3 web.py $EXCLUDE_ID
else
  python3 web.py
fi
