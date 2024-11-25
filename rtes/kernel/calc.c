#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/syscalls.h>
#include <linux/math64.h>
#include <linux/rtes_framework.h>

const u16 DECIMAL_MASK = (1 << 10) - 1;
const u32 WHOLE_MASK = (1 << 16) - 1;
const s32 PARSE_FAIL = -1;
const s32 WHOLE_MULTIPLIER = 1000;
const s32 MAX_DECIMAL_PLACES = 3;

#define INC(N) if (decimal_seen) { decimal *= 10; decimal += (N); decimal_count++; } \
else{whole *= 10; whole += (N); } 

// Returns WHOLE_PART * 1000 + DECIMAL_PART
// Returns a negative number upon failure
// Has 16 + 10 == 26 significant bits 
s32 parse_param(const char* param) {
	char *c;
	size_t decimal_count;
	u32 whole;
	u16 decimal;
	bool decimal_seen;


	if (param == NULL) 
	{
		printk(KERN_INFO "Parameter is null!\n");
		return PARSE_FAIL;
	}
	c = (char*) param;
	decimal_seen = false;
	decimal_count = 0;
	whole = 0;
	decimal = 0;
	while (c && *c) {
		switch (*c) {
			case '.':
				if (decimal_seen) 
				{
					printk(KERN_INFO "Decimal has been seen already!\n");
					return PARSE_FAIL;
				}
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
		if (whole > WHOLE_MASK) 
		{
			printk(KERN_INFO "Exceeding max value!\n");
			return PARSE_FAIL;
		}
		if (decimal > DECIMAL_MASK)
		{ 
			printk(KERN_INFO "Exceeding Decimal mask!\n");
			return PARSE_FAIL;
		}
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

s64 parse_calc_data(struct calc_data c) {
	u32 whole;
	u16 decimal;

	whole = ((u32) c.whole) & WHOLE_MASK;
	decimal = c.decimal & DECIMAL_MASK;

	return (whole * WHOLE_MULTIPLIER) + decimal;
}

s64 reverse_whole(s64 num) {
	s64 out = 1; // Need extra 1 to account for trailing 0s
	s32 rem;
	while (num > 0) {
		out *= 10;
		num = div_s64_rem(num, 10, &rem);
		out += rem;
	}
	return out;
}

s64 reverse_decimal(s64 dec) {
	size_t i;
	s64 out = 0; 
	s32 rem;

	for (i = 0; i < MAX_DECIMAL_PLACES; i++) {
		out *= 10;
		dec = div_s64_rem(dec, 10, &rem);
		out += rem;
	}
	return out;
}

void write_result(char* result_buf, s64 result) {
	size_t i;
	s64 whole_part, reversed_whole;
	s32 decimal_part, reversed_decimal, rem;
	char value;


	whole_part = div_s64_rem(result, WHOLE_MULTIPLIER, &decimal_part);

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

void print_negative(char* result_buf, s64 x) {
	s64 neg;

	neg = ~x + 1;
	*result_buf = '-';
	result_buf++;
	write_result(result_buf, neg);
}

void save_calc_struct(struct calc_data* result, s64 x) {
	u64 whole_part;
	u32 decimal_part;

	result->negative = false;
	if (x < 0) {
		x = ~x + 1;
		result->negative = true;
	}

	whole_part = div_u64_rem(x, WHOLE_MULTIPLIER, &decimal_part);
	result->whole = (u16) whole_part;
	result->decimal = (u16) decimal_part;
}

int do_calc(s64 p1, s64 p2, char operation) {
	s64 output = 0;
	switch (operation) {
		case '+': 
			output = p1 + p2;
			break;
		case '-': 
			output = p1 - p2;
			break;
		case '*': 
			output = div64_s64(p1 * p2, WHOLE_MULTIPLIER);
			break;
		case '/': 
			if (p2 == 0) return -EINVAL;
			output = div64_s64(p1 * WHOLE_MULTIPLIER, p2);
			break;
		default:
			return -EINVAL;
	}

	return output;
}

SYSCALL_DEFINE4(calc, 
		const char*, param1, 
		const char*, param2, 
		char, operation, 
		char*, result) {
	s64 p1, p2, output;

	if (result == NULL) return EINVAL;
	// Need a check that result_buf is long enough? 

	p1 = parse_param(param1);
	if (p1 < 0) return EINVAL;

	p2 = parse_param(param2);
	if (p2 < 0) return EINVAL;

	output = do_calc(p1, p2, operation);
	if (output == -EINVAL) {
		return EINVAL;
	} else if (output < 0) {
		print_negative(result, output);
	} else {
		write_result(result, output);
	}

	return 0;
}

int structured_calc(
	struct calc_data param1, 
	struct calc_data param2, 
	char operation, 
	struct calc_data* result
) {
	s64 p1, p2, output;
	if (result == NULL) return EINVAL;

	p1 = parse_calc_data(param1);
	if (p1 < 0) return EINVAL;
	p2 = parse_calc_data(param2);
	if (p2 < 0) return EINVAL;

	output = do_calc(p1, p2, operation);
	if (output == -EINVAL) {
		return EINVAL;
	}
	save_calc_struct(result, output);

	return 0;
}

