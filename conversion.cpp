#include "conversion.h"

template <class T>
ap_int<16> conv_round (T val) {
	if (val >= 0)
		return (ap_int<16>)(val + static_cast<T>(0.5));
	else return (ap_int<16>)(val - static_cast<T>(0.5));
}

ap_int<16> xray_convert(ap_uint<16> input,
		type_g0 g0, type_g1 g1, type_g2 g2,
		type_p  p0, type_p  p1, type_p  p2) {

	// For a 16-bit pixel read-out:
	//    -  2 bits for mode of the detector (00: mode 0, 01: mode 1, 11: mode 2, 10: impossible)
	//    - 14 bits for accumulated charge, that need subtraction of constant p and division by constant g

	// There are 6 conversion constants:
	//    - Constants p0,p1,p2 are always in range 0 - 16384
	//    - Constant g0 is always in range 0.0313 - 0.0625
	//    - Constant g1 is always in range 0.2500 - 0.5000
	//    - Constant g2 is always in range 1.0000 - 2.0000

	ap_uint<2> mode = input(15,14);
	type_p     adu = input(13,0);
	switch (mode) {
	case 0:
		return conv_round((adu - p0) * g0);
	case 1:
		return conv_round((adu - p1) * g1);
	case 3:
		return conv_round((adu - p2) * g2);
	default:
		return -32676; // just indication that wrong value is placed
	}
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

void convert_packet(ap_uint<512> *host_mem_in, ap_uint<512> *host_mem_out,
					type_g0 *g0, type_g1 *g1, type_g2 *g2,
					type_p *p0, type_p *p1, type_p *p2,
					action_reg *act) {
#pragma HLS INTERFACE s_axilite port=act offset=0x80
#pragma HLS INTERFACE m_axi depth=512 port=host_mem_in  offset=off
#pragma HLS INTERFACE m_axi depth=512 port=host_mem_out offset=off
#pragma HLS INTERFACE m_axi depth=512 port=p0 offset=off
#pragma HLS INTERFACE m_axi depth=512 port=p1 offset=off
#pragma HLS INTERFACE m_axi depth=512 port=p2 offset=off
#pragma HLS INTERFACE m_axi depth=512 port=g2 offset=off
#pragma HLS INTERFACE m_axi depth=512 port=g0 offset=off
#pragma HLS INTERFACE m_axi depth=512 port=g1 offset=off

	for (int i = 0; i < act->npackets; i++) {
		ap_uint<16> input[32], output[32];
		unpack(host_mem_in[act->offset_in  / 64 + i], input);

		for (int j = 0; j < 32; j++)
			output[j] = xray_convert(input[j], g0[i * 32 + j], g1[i * 32 + j], g2[i * 32 + j],
									 p0[i * 32 + j], p1[i * 32 + j], p2[i * 32 + j]);

		pack(host_mem_out[act->offset_out / 64 + i], output);
	}
}
