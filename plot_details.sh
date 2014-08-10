#!/bin/bash
#
#   plot_details.sh
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


# A demo script to run test and plot the figure of [response_time|IOPS|Throughput]
# on a timeline.
# This helps to understand the performance consistency during long time test
# The standard deviation can also be plotted, will be added later.


# test duration for each case, in second, the longer the better, 
# especially for testing the write performance of SSD
duration=$1

# time: plot response time (default), iops: plot IOPS, throughput: plot throughput (MB/s)
measure=$2
if [ "$measure" = "" ]; then
  measure="time"
fi

sector_num="8"

basedir=`readlink -f $0 2>/dev/null| xargs dirname`
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
  echo "Neither make nor gcc is found. Cannot compile iobench."
  exit
fi

output_dir="$basedir/output-detail-`date +%Y%m%d-%H%M%S`"
rm -rf $output_dir/* &> /dev/null
if [ ! -d "$output_dir" ]; then
    mkdir -p $output_dir
fi

log_file="$output_dir/plot_details.log"

echo "testing r50w50 ..." | tee -a $log_file
output_file="$output_dir/io_detail_50r50w"
$basedir/iobench -d ${duration:-10} -P -o $output_file -r -w 50 -p ${sector_num}
sleep 1

echo "collecting data ..." | tee -a $log_file

plot_data_file="io_detail_50r50w.dat"
plot_data_file_full_path="$output_dir/$plot_data_file"

grep 'io' $output_file | awk -F, '{
if ($3=="w") print $2",-,"$NF;
if ($3=="r") print $2","$NF",-";
}' > ${plot_data_file_full_path}

plot_cmd_file="io_detail_50r50w.plot"

case $measure in
  'time') 
    y_label="latency (us)"
    measure_r="2"
    measure_w="3"
    ;;
  'iops')
    y_label="IOPS"
    measure_r="(1000000/\$2)"
    measure_w="(1000000/\$3)"
    ;;
  'throughput')
    y_label="Throughput (MB/s)"
    page_size=$(( $sector_num * 512 ))
    measure_r="(${page_size}*1000000/\$2/1024/1024)"
    measure_w="(${page_size}*1000000/\$3/1024/1024)"
    ;;
  *)
    y_label="latency (us)"
    measure_r="2"
    measure_w="3"
    ;; 
esac

cat << EOF >> $output_dir/$plot_cmd_file

set terminal png size 640,540

set xlabel "time (MM:SS)"
set xdata time
set timefmt "%s"
set format x "%M:%S"

set grid ytics
set ytics

set key outside center top horizontal
set ylabel "$y_label"
set output "iobench-time-detail.png"

set datafile separator ","

plot \
"$plot_data_file" u 1:$measure_r t "rnd_read" w points lt 3 pt 1, \
"$plot_data_file" u 1:$measure_w t "rnd_write" w points lt 1 pt 2

EOF

if ! hash "gnuplot" &> /dev/null; then
    echo "gnuplot not found, cannot plot figures. please plot it by yourself"
    echo "with data file $plot_data_file and commands file $plot_cmd_file in $output_dir."
    exit
fi

sleep 1

echo "ploting ..." | tee -a $log_file
cd $output_dir && gnuplot $plot_cmd_file

case `uname -s` in
  'Darwin')
    open -a Preview "$output_dir/iobench-time-detail.png"
    ;;
  'Linux')
    if ! hash "eog" &> /dev/null; then
      echo "eog not found. Please use your favorite tool to view figures. "
      exit
    fi
    eog "$output_dir/iobench-time-detail.png" &
    ;;
  *)
    echo 'Unknown OS. Please use your favorite tool to view the figures.'
esac


