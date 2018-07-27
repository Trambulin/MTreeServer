#ifndef SHA256ABSTRACT_H
#define SHA256ABSTRACT_H
#include<stdint.h>

#define bswap_32(x) ((((x) << 24) & 0xff000000u) | (((x) << 8) & 0x00ff0000u) \
                   | (((x) >> 8) & 0x0000ff00u) | (((x) >> 24) & 0x000000ffu))

class sha256Abstract
{
private:
    sha256Abstract();

public:
    static uint32_t be32dec(const void *pp);
    static void sha256_transform(uint32_t *state, const uint32_t *block, int swap);
    static void sha256d(unsigned char *hash, const unsigned char *data, int len);
};

#endif