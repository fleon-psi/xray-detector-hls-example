# xray-detector-hls-example

This is an example of high-level synthesis (HLS) code, that could be implemented as OC-Accel action. 
It does a very simplified implementation of conversion procedure for integrating X-ray detector, without implementing multiple gain modes. 
For details see publication by Leonarski et al. in *Structural Dynamics* [(link)](https://aca.scitation.org/doi/full/10.1063/1.5143480). 

## Excersises
First, please have a look on the code in `conversion.cpp` file. It is all valid C++ code, but you will find multiple `#pragma HLS` lines. These lines tell the HLS compiler information that cannot be describe by pure C/C++ constructs - mostly explaining hardware implementation details.
There is nice feature of C/C++ language design - pragma unknown to compiler is just ignored, without warning or error. Therefore all these statements will be just ignored, if the code is compiled with gcc or clang.

### 1. Test code on CPU
First you can check, if the code can be compiled and tested on a CPU, as standard C++ code. For this go to the directory with the example and run:
```
$ g++ -otest_bench -std=c++11 test_bench.cpp conversion.cpp -I<Vivado 2019.2 path>/include/
$ ./test bench
```
You should expect the following output (there might some ap integer warnings - this should be disregarded):
```
error = 0.288649 (0.25 is ideal value for integer rounding; less than 0.35 is good enough)
```
The value is root mean square deviation between values calculated by reference C++ routine and the HLS code. The numbers are always rounded to integers, so error in range around 0.25 comes simply from truncation and is expected.

### 2. Synthesize code for FPGA and analyze results
Next you can try, if the code can be converted to hardware design language by HLS compiler. Therefore type:
```
$ vivado_hls -f hls.tcl
```
This will take a bit of time (and messages) to run HLS synthesis. In subfolder `conversion-example/solution1/syn/verilog` you can find output in hardware design language. 
The most interesting information is however found in `conversion-example/solution1/syn/report` folder, where reports from HLS synthesis are found. These report tell us performance and resource utilization of HLS routine.
Reports are produced in both human readable text format (`.rpt`) and machine readable XML format (`.xml`). Reports are generated for each function seperately, unless function was inlined by HLS compiler. 

First look in `conversion-example/solution1/syn/report/hls_action_csynth.rpt` file. There will be an important table, that provides information about resources used:

```
================================================================
== Utilization Estimates
================================================================
* Summary: 
+-----------------+---------+-------+--------+--------+-----+
|       Name      | BRAM_18K| DSP48E|   FF   |   LUT  | URAM|
+-----------------+---------+-------+--------+--------+-----+
|DSP              |        -|      -|       -|       -|    -|
|Expression       |        -|      -|       -|       -|    -|
|FIFO             |        -|      -|       -|       -|    -|
|Instance         |       60|     14|    9602|   14149|    0|
|Memory           |        0|      -|       0|       0|  128|
|Multiplexer      |        -|      -|       -|     350|    -|
|Register         |        -|      -|     305|       -|    -|
+-----------------+---------+-------+--------+--------+-----+
|Total            |       60|     14|    9907|   14499|  128|
+-----------------+---------+-------+--------+--------+-----+
|Available        |     1344|   2880|  879360|  439680|  320|
+-----------------+---------+-------+--------+--------+-----+
|Utilization (%)  |        4|   ~0  |       1|       3|   40|
+-----------------+---------+-------+--------+--------+-----+
```
This table gives an advice, how many resources on the FPGA will be necessary to implement the function. Please note - these are only estimates + the higher utilization you aim for, the more difficult it will be for the synthesis tool to implement the design with proper timing, so don't aim at 99% utilization. 

The other important information is performance of conversion routine. Actually the example action is composed of two routines - load constants from memory and conversion. In real application the first one will be only loaded once at the beginning of operation, while the latter will execute constantly. Therefore main aim for optimization is looking into the latter one.
So please open `conversion-example/solution1/syn/report/convert_csynth.rpt` file. There is a table that describes loop performance:
```
        * Loop:
        +-----------+---------+---------+----------+-----------+-----------+------+----------+
        |           |  Latency (cycles) | Iteration|  Initiation Interval  | Trip |          |
        | Loop Name |   min   |   max   |  Latency |  achieved |   target  | Count| Pipelined|
        +-----------+---------+---------+----------+-----------+-----------+------+----------+
        |- main     |        ?|        ?|      9380|          -|          -|     ?|    no    |
        | + main.1  |	   512|      512|         1|          -|          -|   512|    no    |
        | + main.2  |     1184|     1184|        37|          -|          -|    32|    no    |
        | + main.3  |     7680|     7680|        15|          -|          -|   512|    no    |
        +-----------+---------+---------+----------+-----------+-----------+------+----------+
```
To make the table more readable it is advised to name loops with `<name>: for () {}` syntax.  
From this table we learn a troubling information - it takes 9380 clock cycles to run one iteration the loop. Given 200 MHz frequence this gives 512 bit / (9380 * 5 ns) = 1.3 MB/s. This is extremly slow, let's improve this...

### 3. Pipeline main loop
We can ask HLS compiler to pipeline main loop. This is done by adding PRAGMA line in conversion.cpp:
```
void convert(ap_uint<512> *pixels_in, ap_uint<512> *pixels_out, type_p0 *p0, type_g0 *g0, uint64_t npackets) {
        main: for (int i = 0; i < npackets ; i++) {
**#pragma HLS PIPELINE **
                        ap_uint<16> input[32], output[32];
                        // One packet is 512 bits, so 32 x 16-bit numbers
                        unpack(pixels_in[i], input);
                        for (int j = 0; j < 32; j++) {
                                output[j] = conv_round((input[j] - p0[i * 32 + j]) * g0[i * 32 + j]);
                        }
                        pack(pixels_out[i], output);
        }
}

```
Now try again:
```
$ vivado_hls -f hls.tcl
```
Now checking `conversion-example/solution1/syn/report/convert_csynth.rpt`  again, we see the performance changed: 
```
        * Loop:
        +----------+---------+---------+----------+-----------+-----------+------+----------+
        |          |  Latency (cycles) | Iteration|  Initiation Interval  | Trip |          |
        | Loop Name|   min   |   max   |  Latency |  achieved |   target  | Count| Pipelined|
        +----------+---------+---------+----------+-----------+-----------+------+----------+
        |- main    |        ?|        ?|        52|         16|          1|     ?|    yes   |
        +----------+---------+---------+----------+-----------+-----------+------+----------+
```
Actually few things happened here:

1. Loop was sucessfully pipelined, as seen by "yes" in the last column.
2. There were three loops embedded within main (unpack, convert, pack) - all these loops were unrolled and merged into main loop, so they execute in parallel. This is why they dissapear from the list. Iteration latency was therefore cut to 52 cycles.
3. There is additional field called `Initiation Interval achieved` - this field tells us how often the the function will ask for new data - this is every 16 cycles.

It is important to distinguish between (2) and (3). Latency tells us how long it takes for the data to pass through the pipeline, while initiation interval tells what is throughput of the pipeline. Simple calculation gives 512 bit / (16 * 5 ns) = 800 MB/s. This is already quite good, but could it do better?

Go back to output of HLS synthesis. There you can find the following hint:
```
WARNING: [SCHED 204-69] Unable to schedule 'load' operation ('p0_load_2', conversion.cpp:63) on array 'p0' due to limited memory ports. Please consider using a memory core with more ports or partitioning the array 'p0'.
```
This tells us that pipeline throughput is limited by memory throughput. Indeed the memory to store conversion constants (`p0` and `g0`) allows to read only one value per clock cycle, while we would like to convert 16 values per clock cycle. Therefore we should ask HLS compiler to partition these memories into smaller memories with separate interfaces.
You can modify the `conversion.cpp` file again to add partition pragmas:
```
        type_p0 p0[N];
#pragma HLS RESOURCE variable=p0 CORE=RAM_2P_URAM
**#pragma HLS ARRAY_PARTITION variable=p0 cyclic factor=16**
        type_g0 g0[N];
#pragma HLS RESOURCE variable=g0 CORE=RAM_2P_URAM
**#pragma HLS ARRAY_PARTITION variable=g0 cyclic factor=16**
```

Please run synteshis again:
```
$ vivado_hls -f hls.tcl
```

Performance table in `conversion-example/solution1/syn/report/convert_csynth.rpt` should be as following:
```
        * Loop:
        +----------+---------+---------+----------+-----------+-----------+------+----------+
        |          |  Latency (cycles) | Iteration|  Initiation Interval  | Trip |          |
        | Loop Name|   min   |   max   |  Latency |  achieved |   target  | Count| Pipelined|
        +----------+---------+---------+----------+-----------+-----------+------+----------+
        |- main    |        ?|        ?|        37|          1|          1|     ?|    yes   |
        +----------+---------+---------+----------+-----------+-----------+------+----------+
```
Congratulations! The loop can now process 512 bits of data per clock cycle, giving performance of 512 bit / 5 ns = 12.8 GB/s. 

### Convert floating point numbers to fixed point numbers
The performance is excellent, but we should check again resource utilization:
```
================================================================
== Utilization Estimates
================================================================
* Summary:
+-----------------+---------+-------+--------+--------+-----+
|	Name	  | BRAM_18K| DSP48E|   FF   |   LUT  | URAM|
+-----------------+---------+-------+--------+--------+-----+
|DSP              |        -|	   -|       -|       -|    -|
|Expression	  |        -|	   -|       -|       -|    -|
|FIFO             |        -|	   -|       -|       -|    -|
|Instance         |	 188|    544|  134120|  120290|    -|
|Memory           |        0|	   -|       0|       0|  128|
|Multiplexer	  |        -|	   -|       -|    1814|    -|
|Register         |        -|	   -|     305|       -|    -|
+-----------------+---------+-------+--------+--------+-----+
|Total            |	 188|    544|  134425|  122104|  128|
+-----------------+---------+-------+--------+--------+-----+
|Available        |     1344|   2880|  879360|  439680|  320|
+-----------------+---------+-------+--------+--------+-----+
|Utilization (%)  |	  13|     18|	   15|      27|   40|
+-----------------+---------+-------+--------+--------+-----+
```
The resource utilization is much higher than before. Why? Because of parallelization, that is achieved by increasing resource use. This simple design is using 27% of look-up tables, 18% of digital signal processing blocks and 15% of flip flops. It will be difficult, it one wants to add any more functionality. 

There is however a way to reduce utilization. Instead of floating point numbers, we can try fixed point numbers. Let's edit `conversion.cpp` and change datatypes, by replacing:
```
typedef double type_p0;
typedef double type_g0;
```
with:
```
typedef ap_ufixed<16, 14, AP_RND_CONV> type_p0;
typedef ap_ufixed<16,  1, AP_RND_CONV> type_g0;
```

This means that p0 will be described by unsigned fixed-point number with 14 integer bits, 2 fractional bits and convergent rounding, while g0 will have 1 integer bit, 15 fractional bits and the same rounding scheme. As an extra exercise you can also change number of fractional bits or rounding (see HLS manual for details), to see how result quality changes. 

The first thing, we should check if qualit of conversion is OK. Let's run again test bench:
```
$ g++ -otest_bench -std=c++11 test_bench.cpp conversion.cpp -I<Vivado 2019.2 path>/include/
$ ./test bench
error = 0.314465 (0.25 is ideal value for integer rounding; less than 0.35 is good enough)
```
Error is a bit higher, but still in reasonable range.

So let's try synthesis:
```
$ vivado_hls -f hls.tcl
```
By looking on output reports, initialization intervals stays the same, but resource usage is significantly lower:
```
================================================================
== Utilization Estimates
================================================================
* Summary:
+-----------------+---------+-------+--------+--------+-----+
|       Name      | BRAM_18K| DSP48E|   FF   |   LUT  | URAM|
+-----------------+---------+-------+--------+--------+-----+
|DSP              |        -|      -|       -|       -|    -|
|Expression       |        -|      -|       -|       -|    -|
|FIFO             |        -|	   -|       -|       -|    -|
|Instance         |	  60|     32|    8364|   10926|    -|
|Memory           |        0|	   -|       0|       0|  128|
|Multiplexer      |        -|      -|       -|    2549|    -|
|Register         |        -|      -|     306|       -|    -|
+-----------------+---------+-------+--------+--------+-----+
|Total            |       60|     32|    8670|   13475|  128|
+-----------------+---------+-------+--------+--------+-----+
|Available        |     1344|   2880|  879360|  439680|  320|
+-----------------+---------+-------+--------+--------+-----+
|Utilization (%)  |        4|	   1|   ~0   |       3|   40|
+-----------------+---------+-------+--------+--------+-----+
```

This ends the excersise, but you are free to explore the code more. 
