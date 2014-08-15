#!/bin/bash
#
#   plot_figures.sh
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

# A demo script to run tests and plot figures for basic IO patterns including 
# sequential/random read/write

# plot script is generated in output directory with data, can be easily
# customized to different figures.

# test duration for each case, in second, the longer the better, 
# especially for testing the write performance of SSD
duration=$1

other_iombench_opts=""

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

output_dir="$basedir/output-`date +%Y%m%d-%H%M%S`"
rm -rf $output_dir/* &> /dev/null
if [ ! -d "$output_dir" ]; then
    mkdir -p $output_dir
fi

log_file="$output_dir/plot_figure.log"

# x labels
x_file="$output_dir/x_file.dat"
sector_size=512
for size in `seq 0 8`; do

    if hash "bc" &> /dev/null; then
      req_size=`echo 2^$size*${sector_size} | bc`

    elif hash "python" &> /dev/null; then
      req_size=`python -c "print 2**$size*${sector_size}"`

    else
      echo "Neither bc nor python is found."
      exit
    fi

    echo $req_size >> $x_file
done

file_list_to_merge="$x_file "

for rp in '' '-r'; do
  for wp in '100' '0'; do
    output_file="$output_dir/io_size_test_w_"$wp$rp

    # req_size 512B to 64KB
    for size in `seq 0 8`; do

        if hash "bc" &> /dev/null; then

          sector_num=`echo 2^$size | bc`

        elif hash "python" &> /dev/null; then

          sector_num=`python -c "print 2**$size"`
        else
          echo "Neither bc nor python is found."
          exit
        fi

        echo "-w $wp $rp -p $sector_num" | tee -a $log_file

        $basedir/iombench -d ${duration:-2} -p $sector_num -o $output_file -w $wp $rp $other_iombench_opts

    done

    sleep 1
    grep 'summary' $output_file | awk '{print $(NF-1)}' > $output_dir/w_$wp$rp.dat
    file_list_to_merge="${file_list_to_merge} $output_dir/w_$wp$rp.dat"

  done
done

plot_data_file="simple4.dat"
plot_data_file_full_path="$output_dir/$plot_data_file"
echo "file list: ${file_list_to_merge}" >> $log_file
paste ${file_list_to_merge} > $plot_data_file_full_path

plot_cmd_file="simple4.plot"

cat << EOF >> $output_dir/$plot_cmd_file

set terminal png size 640,540

set xlabel "Request Size (KB)"
set grid ytics
set ytics

set key outside center top horizontal

set ylabel "time (us)"
set output "iombench-time.png"
plot \
"$plot_data_file" u 1:3 t "seq_read" w linespoints lt 3 pt 1, \
"$plot_data_file" u 1:2 t "seq_write" w linespoints lt 1 pt 2, \
"$plot_data_file" u 1:5 t "rnd_read" w linespoints lt 2 pt 3, \
"$plot_data_file" u 1:4 t "rnd_write" w linespoints lt 4 pt 4

set logscale x
set xtics nomirror ( \
"0.5" 512,\
"1" 1024,\
"2" 2048,\
"4" 4096,\
"8" 8192,\
"16" 16384,\
"32" 32768,\
"64" 65536,\
"128" 131072)

set ylabel "Throughput (MB/s)"
set output "iombench-seq-thrpt.png"
plot \
"$plot_data_file" u 1:(\$1*1000000/\$3/1024/1024) t "seq_read" w linespoints lt 3 pt 1, \
"$plot_data_file" u 1:(\$1*1000000/\$2/1024/1024) t "seq_write" w linespoints lt 1 pt 2

set ylabel "IOPS"
set output "iombench-rnd-iops.png"
plot \
"$plot_data_file" u 1:(1000000/\$5) t "rnd_read" w linespoints lt 3 pt 1, \
"$plot_data_file" u 1:(1000000/\$4) t "rnd_write" w linespoints lt 1 pt 2

EOF

if ! hash "gnuplot" &> /dev/null; then
    echo "gnuplot not found, cannot plot figures. please plot it by yourself"
    echo "with data file $plot_data_file and commands file $plot_cmd_file in $output_dir."
    exit
fi

sleep 1

cd $output_dir && gnuplot $plot_cmd_file

case `uname -s` in
  'Darwin')
    open -a Preview $output_dir/*.png
    ;;
  'Linux')
    if ! hash "eog" &> /dev/null; then
      echo "eog not found. Please use your favorite tool to view figures. "
      exit
    fi
    eog $output_dir/*.png &
    ;;
  *)
    echo 'Unknown OS. Please use your favorite tool to view the figures.'
esac

