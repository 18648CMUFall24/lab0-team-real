#include <linux/kernel.h>

const uint16_t DECIMAL_MASK = (1 << 10) - 1;
const uint32_t WHOLE_MASK = (1 << 16) - 1;
const int32_t PARSE_FAIL = -1;

#define INC(N) if (decimal_seen) { decimal *= 10; decimal += (N); decimal_count++; } \
		else{whole *= 10; whole += (N); } break

// Returns WHOLE_PART << 10 + DECIMAL_PART
// Returns a negative number upon failure
// Has 16 + 10 == 26 significant bits
int32_t parse_param(const char* param) {
	if (param == NULL) return PARSE_FAIL;

	char *c = param;
	bool decimal_seen = false;
	bool decimal_count = 0;
	uint32_t whole = 0;
	uint16_t decimal = 0;
	while (c && *c) {
		switch (*c) {
			case '.':
				if (decimal_seen) return PARSE_FAIL;
				decimal_seen = true; 
				break;
			case '0': INC(0);
			case '1': INC(1);
			case '2': INC(2);
			case '3': INC(3);
			case '4': INC(4);
			case '5': INC(5);
			case '6': INC(6);
			case '7': INC(7);
			case '8': INC(8);
			case '9': INC(9);
			default: return PARSE_FAIL;
		}

		// Fail if exceeding max values
		if (whole > WHOLE_MASK) return PARSE_FAIL;
		if (decimal > DECIMAL_MASK) return PARSE_FAIL;
		c++;
	}

	decimal_seen = true;
	while (decimal_count < 3) {
		INC(0);
	}

	return ((whole & WHOLE_MASK) << 10) + (decimal & DECIMAL_MASK);
}

int64_t reverse_number(int64_t num, int8_t num_bits) {
	int64_t out = 0;
	while (num > 0) {
		out *= 10;
		out += num % 10;
		num /= 10;
	}
	return out;
}

int write_result(char* result_buf, int64_t result, int8_t whole_bits, int8_t decimal_bits) {
	int64_t decimal_mask = (1 << decimal_bits) - 1;
	int64_t decimal_part = result & decimal_mask;
	int64_t whole_part = result >> decimal_bits;

	int64_t reversed_whole = reverse_number(whole_part);
	int64_t reversed_decimal = reverse_number(decimal_part); 

	while (revesed_deci

	while (reversed_whole > 0) {
		*result_buf = '0' + (reversed_whole % 10);
		reversed_whole /= 10;
		result_buf++;
	}

	*result_buf = '.';
	result_buf++;

	while () {
	}


}

int sys_calc(const char* param1, const char* param2, char operation, char* result) {
	if (result == NULL) return EINVAL;

	int64_t p1 = parse_param(param1);
	if (p1 < 0) return EINVAL;

	int64_t p2 = parse_param(param1);
	if (p2 < 0) return EINVAL;

	int64_t output = 0;
	int32_t decimal_bits = 10;
	switch (operation) {
		case '+': // Requires 26 + 1 == 27 bits (10 decimal)
			output = p1 + p2;
			break;
		case '-': // Requires 26 Bits (10 decimal) unless negative
			if (p2 > p1) return EINVAL;
			output = p1 - p2;
			break;
		case '*': // Requires 2 * 26 = 52 bits (20 decimal)
			output = p1 * p2;
			decimal_bits = 20;
			break;
		case '/': // Requires 26 + 10 = 36 bits (10 decimal)
			if (p2 == 0) return EINVAL;
			output = (p1 << decimal_bits) / p2;
			break;
		default:
			return EINVAL;
	}
}
