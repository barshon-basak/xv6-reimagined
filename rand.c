// rand.c: Simple pseudo-random number generator for xv6 lottery scheduling

static unsigned short lfsr = 0xACE1u;

unsigned short rand(void)
{
    unsigned short bit;
    bit  = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1u;
    lfsr = (lfsr >> 1) | (bit << 15);
    return lfsr;
}

// Returns a random number in range [0, max] inclusive
long random_at_most(long max)
{
    if (max <= 0)
        return 0;

    unsigned long num_bins = (unsigned long)max + 1;
    unsigned long result;

    // Simple rejection sampling to reduce bias
    do {
        result = rand();
    } while (result >= (65536UL - (65536UL % num_bins)));

    return (long)(result % num_bins);
}
