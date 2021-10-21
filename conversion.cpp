#include "conversion.h"

// Rounding of fraction to closest integer (standard truncation will introduce error)
template <class T>
ap_int<16> conv_round (T val) {
#pragma HLS INLINE
	if (val >= 0)
		return (ap_int<16>)(val + static_cast<T>(0.5));
	else return (ap_int<16>)(val - static_cast<T>(0.5));
}

// Copy bit-to-bit int64 to double (no conversion)
// This trick is often necessary in HLS, if one reads word as ap_uint<512>, and wants to read individual bytes
double bit_int2double(uint64_t input) {
#pragma HLS INLINE
	union fp_int_union {
		uint64_t int_val;
		double fp_val;
	} x;
	x.int_val = input;
	return x.fp_val;
}


// These functions pack/unpack any 16-bit arbitrary precision format to 512-bit
template <class T>
void unpack(ap_uint<512> input, T output[32]) {
#pragma HLS INLINE
	for (int i = 0; i < 512; i++)
			output[i / 16][i % 16] = input[i];
}

template <class T>
void pack(ap_uint<512> &output, T input[32]) {
#pragma HLS INLINE
	for (int i = 0; i < 512; i++)
		output[i] = input[i / 16][i % 16];
}

// Functions to load constants from memory
void load(ap_uint<512> *input, double *out) {
	for (int i = 0; i < N / 8; i++) {
		ap_uint<512> tmp = input[i];
		for (int j = 0; j < 8; j++) {
			out[i * 8 + j] = bit_int2double(tmp(j * 64 + 63, j * 64));
		}
	}
}

template <class T>
void load(ap_uint<512> *input, T *out) {
	for (int i = 0; i < N /32 ; i++)
		unpack(input[i], out + i * 32);
}

void convert(ap_uint<512> *pixels_in, ap_uint<512> *pixels_out, type_p0 *p0, type_g0 *g0, uint64_t npackets) {
	main: for (int i = 0; i < npackets ; i++) {
			ap_uint<16> input[32], output[32];
			// One packet is 512-bits, so 32 x 16-bit numbers
			unpack(pixels_in[i], input);
			for (int j = 0; j < 32; j++) {
				output[j] = conv_round((input[j] - p0[i * 32 + j]) * g0[i * 32 + j]);
			}
			pack(pixels_out[i], output);
		}
}

void hls_action(ap_uint<512> *host_mem_in, ap_uint<512> *host_mem_out, action_reg *act) {
#pragma HLS INTERFACE s_axilite port=return bundle=ctrl_reg
#pragma HLS INTERFACE s_axilite port=act bundle=ctrl_reg offset=0x100
#pragma HLS DATA_PACK variable=act
	
#pragma HLS INTERFACE m_axi depth=512 bundle=host_mem port=host_mem_in  offset=off
#pragma HLS INTERFACE m_axi depth=512 bundle=host_mem port=host_mem_out offset=off

	type_p0 p0[N];
#pragma HLS RESOURCE variable=p0 CORE=RAM_2P_URAM
	type_g0 g0[N];
#pragma HLS RESOURCE variable=g0 CORE=RAM_2P_URAM

	load(host_mem_in + act->offset_p0 / 64, p0);
	load(host_mem_in + act->offset_g0 / 64, g0);

	convert(host_mem_in + act->offset_in / 64, host_mem_out + act->offset_out / 64, p0, g0, act->npackets);
}
