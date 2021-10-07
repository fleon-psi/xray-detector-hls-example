# xray-detector-hls-example

## Make test bench
g++ -std=c++11 test_bench.cpp conversion.cpp -I<Vivado 2019.2 path>/include/

## Make HLS synthesis
vivado_hls -f hls.tcl

## Print HLS statistics for the main function
cat conversion-example/solution1/syn/report/convert_packet_csynth.rpt
