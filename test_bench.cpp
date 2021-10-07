#include <stdint.h>
#include <random>
#include <sys/mman.h>

#include "conversion.h"

#define N (512*1024)

int main(int argc, char **argv) {
	// Allocate memory - page aligned
	uint16_t *input_data = (uint16_t *)mmap(nullptr, N * sizeof(uint16_t), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	int16_t *output_data = (int16_t *) mmap(nullptr, N * sizeof(int16_t) , PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	// Allocate memory (alignment not relevant)
	std::vector<double> fp_p0(N), fp_p1(N), fp_p2(N), fp_g0(N), fp_g1(N), fp_g2(N), ref(N);

	std::vector<type_p>  p0(N), p1(N), p2(N);
	std::vector<type_g0> g0(N);
	std::vector<type_g1> g1(N);
	std::vector<type_g2> g2(N);

	// Generate random input, p0, p1, p2, g0, g1, g2 arrays
	// But we use preset random generator seed, so the result is always the same
    std::mt19937 random_gen(16781);
    std::uniform_int_distribution<uint16_t> dist;

    std::uniform_real_distribution<double> p_dist(0, 16383);
    std::uniform_real_distribution<double> g0_dist(1/32., 1/16.);
    std::uniform_real_distribution<double> g1_dist(1/8., 1/4.);
    std::uniform_real_distribution<double> g2_dist(1., 2.);

	for (int i = 0; i < N; i++) {
		input_data[i] = dist(random_gen);

		fp_p0[i] = p_dist(random_gen);
		fp_p1[i] = p_dist(random_gen);
		fp_p2[i] = p_dist(random_gen);

		fp_g0[i] = g0_dist(random_gen);
		fp_g1[i] = g1_dist(random_gen);
		fp_g2[i] = g2_dist(random_gen);
	}

	// Calculate reference solution
	for (int i = 0; i < N; i++) {
		uint16_t mode = input_data[i] & 0xc000;
		double adu    = input_data[i] & 0x3fff;
		switch (mode) {
		case 0:
			ref[i] = (adu - fp_p0[i]) * fp_g0[i];
			break;
		case 0x4000:
			ref[i] = (adu - fp_p1[i]) * fp_g1[i];
			break;
		case 0xc000:
			ref[i] = (adu - fp_p2[i]) * fp_g2[i];
			break;
		default:
			ref[i] = -32676;
			break;
		}
	}

	// Cast constants to whatever type used on FPGA
	for (int i = 0; i < N; i++) {
		p0[i] = fp_p0[i]; p1[i] = fp_p1[i]; p2[i] = fp_p2[i];
		g0[i] = fp_g0[i]; g1[i] = fp_g1[i]; g2[i] = fp_g2[i];
	}

	// Set up action register
	action_reg reg;
	reg.npackets = N / 32; // Each packet contains 512-bits, so 32 x 16-bit numbers
	reg.offset_in = (uint64_t) input_data;
	reg.offset_out = (uint64_t) output_data;
	convert_packet(nullptr, nullptr, g0.data(), g1.data(), g2.data(), p0.data(), p1.data(), p2.data(), &reg);

	// Calculate root mean square deviation (error measure)
	double err = 0;
	for (int i = 0; i < N; i++)
		err += (ref[i] - output_data[i]) * (ref[i] - output_data[i]);
	err = sqrt(err/N);
	std::cout << "RMSD = " << err << " (0.25 is ideal value for integer rounding; 0.30 is good enough)" << std::endl;

	// Deallocate memory
	munmap( input_data, N * sizeof(uint16_t));
	munmap(output_data, N * sizeof( int16_t));

	if (err < 0.5) return EXIT_SUCCESS;
	else return EXIT_FAILURE;
}
