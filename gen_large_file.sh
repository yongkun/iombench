#!/bin/bash
#
#   gen_large_file.sh
#
#   Copyright 2014 Yongkun Wang
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.


if [ $# -lt 1 ]; then
  echo "Usage: $0 file_size_in_GB [file_name]"
  exit
fi

file_size_gb=$1
file_name=$2

case `uname -s` in
  'Darwin')
    available_gb=`df -g . | tail -1 | awk '{print $(NF-2)}'`
    ;;
  'Linux')
    available_gb=`df -k . | tail -1 | awk '{printf "%d",$(NF-2)/1024/1024}'`
    ;;
  *)
    echo 'Unknown OS. Will create the file without checking disk space.'
    available_gb=1048576
esac

if [ $file_size_gb -gt $available_gb ]; then
  echo "Not enough disk space."
  exit
fi

file_size=$(( $file_size_gb * 1024 ))

echo "creating $file_size_gb GB file ${file_name:-testfile.tmp} ..."

# dd if=/dev/zero of=${file_name:-testfile.tmp} bs=1048576 count=${file_size}

basedir=`readlink -f $0 2>/dev/null | xargs dirname`
if [ "$basedir" = "" ]; then
  curr_dir=$(echo "${0%/*}")
  basedir=`(cd "$curr_dir" && echo "$(pwd -P)")`
fi

cd $basedir
if hash "make" &> /dev/null; then
  make
elif hash "gcc" &> /dev/null; then
  ./compile.sh
else
  echo "Neither make nor gcc is found. Cannot compile iombench."
  exit
fi

# -p 2048, page size 1MB (2048x512)
$basedir/iombench -w 100 -p 2048 -n $file_size -S 1000000000000 -f ${file_name:-testfile.tmp}

