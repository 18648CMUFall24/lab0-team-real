#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/syscalls.h>
#include <linux/math64.h>

const uint16_t DECIMAL_MASK = (1 << 10) - 1;
const uint32_t WHOLE_MASK = (1 << 16) - 1;
const int32_t PARSE_FAIL = -1;
const int32_t WHOLE_MULTIPLIER = 1000;
const int32_t MAX_DECIMAL_PLACES = 3;

#define INC(N) if (decimal_seen) { decimal *= 10; decimal += (N); decimal_count++; } \
else{whole *= 10; whole += (N); } 

// Returns WHOLE_PART * 1000 + DECIMAL_PART
// Returns a negative number upon failure
// Has 16 + 10 == 26 significant bits 
int32_t parse_param(const char* param) {
	char *c;
	size_t decimal_count;
	uint32_t whole;
	uint16_t decimal;
	bool decimal_seen;


	if (param == NULL) return PARSE_FAIL;

	c = (char*) param;
	decimal_seen = false;
	decimal_count = 0;
	whole = 0;
	decimal = 0;
	while (c && *c) {
		switch (*c) {
			case '.':
				if (decimal_seen) return PARSE_FAIL;
				decimal_seen = true; 
				break;
			case '0': INC(0) break;
			case '1': INC(1) break;
			case '2': INC(2) break;
			case '3': INC(3) break;
			case '4': INC(4) break;
			case '5': INC(5) break;
			case '6': INC(6) break;
			case '7': INC(7) break;
			case '8': INC(8) break;
			case '9': INC(9) break;
			default: return PARSE_FAIL;
		}

		// Fail if exceeding max values
		if (whole > WHOLE_MASK) return PARSE_FAIL;
		if (decimal > DECIMAL_MASK) return PARSE_FAIL;
		if (decimal_count >= MAX_DECIMAL_PLACES) break;
		c++;
	}

	decimal_seen = true;
	while (decimal_count < MAX_DECIMAL_PLACES) {
		INC(0);
	}

	whole &= WHOLE_MASK;
	decimal &= DECIMAL_MASK;

	return (whole * WHOLE_MULTIPLIER) + decimal;
}

int64_t reverse_whole(int64_t num) {
	int64_t out = 1; // Need extra 1 to account for trailing 0s
	int32_t rem;
	while (num > 0) {
		out *= 10;
		num = div_s64_rem(num, 10, &rem);
		out += rem;
	}
	return out;
}

int64_t reverse_decimal(int64_t dec) {
	size_t i;
	int64_t out = 0; 
	int32_t rem;

	for (i = 0; i < MAX_DECIMAL_PLACES; i++) {
		out *= 10;
		dec = div_s64_rem(dec, 10, &rem);
		out += rem;
	}
	return out;
}

void write_result(char* result_buf, int64_t result, size_t decimal_mask) {
	size_t i;
	int64_t whole_part, reversed_whole;
	int32_t decimal_part, reversed_decimal, rem;
	char value;


	whole_part = div_s64_rem(result, decimal_mask, &decimal_part);

	reversed_whole = reverse_whole(whole_part);
	reversed_decimal = reverse_decimal(decimal_part); 

	// The one prevents trailing 0s from getting dropped
	while (reversed_whole > 1) {
		reversed_whole = div_s64_rem(reversed_whole, 10, &rem);
		value = '0' + rem;
		*result_buf = value;
		result_buf++;
	}

	*result_buf = '.';
	result_buf++;

	for (i = 0; i < MAX_DECIMAL_PLACES; i++) {
		reversed_decimal = div_s64_rem(reversed_decimal, 10, &rem);
		value = '0' + rem;
		*result_buf = value;
		result_buf++;
	}
}

void print_negative(char* result_buf, int64_t x) {
	int64_t neg;

	neg = ~x + 1;
	*result_buf = '-';
	result_buf++;
	write_result(result_buf, neg, MAX_DECIMAL_PLACES);
}

int do_calc(const char* param1, const char* param2, char operation, char* result) {
	int64_t p1, p2, output;

	if (result == NULL) return EINVAL;
	// Need a check that result_buf is long enough? 

	p1 = parse_param(param1);
	if (p1 < 0) return EINVAL;

	p2 = parse_param(param2);
	if (p2 < 0) return EINVAL;

	output = 0;
	switch (operation) {
		case '+': 
			output = p1 + p2;
			write_result(result, output, WHOLE_MULTIPLIER);
			break;
		case '-': 
			output = p1 - p2;
			if (p2 > p1) print_negative(result, output);
			else write_result(result, output, WHOLE_MULTIPLIER);
			break;
		case '*': 
			output = p1 * p2;
			write_result(result, output, WHOLE_MULTIPLIER * WHOLE_MULTIPLIER);
			break;
		case '/': 
			if (p2 == 0) return EINVAL;
			output = div64_s64(p1 * WHOLE_MULTIPLIER, p2);
			write_result(result, output, WHOLE_MULTIPLIER);
			break;
		default:
			return EINVAL;
	}

	return 0;
}

SYSCALL_DEFINE4(calc, const char*, param1, const char*, param2, char, operation, char*, result) 
{
	return do_calc(param1, param2, operation, result);
}

