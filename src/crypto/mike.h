#include <crypto/sph_blake.h>
#include <crypto/sph_bmw.h>
#include <crypto/sph_groestl.h>
#include <crypto/sph_jh.h>
#include <crypto/sph_keccak.h>
#include <crypto/sph_skein.h>
#include <crypto/sph_luffa.h>
#include <crypto/sph_cubehash.h>
#include <crypto/sph_shavite.h>
#include <crypto/sph_simd.h>
#include <crypto/sph_echo.h>


template<typename T1>
inline uint256 HashMike(const T1 pbegin, const T1 pend)

{
    sph_blake512_context     ctx_blake;
    sph_bmw512_context       ctx_bmw;
    sph_groestl512_context   ctx_groestl;
    sph_jh512_context        ctx_jh;
    sph_keccak512_context    ctx_keccak;
    sph_skein512_context     ctx_skein;
    sph_luffa512_context     ctx_luffa;

    static unsigned char pblank[1];

    uint512 hash[10];

    sph_blake512_init(&ctx_blake);
    sph_blake512 (&ctx_blake, (pbegin == pend ? pblank : static_cast<const void*>(&pbegin[0])), (pend - pbegin) * sizeof(pbegin[0]));
    sph_blake512_close(&ctx_blake, static_cast<void*>(&hash[0]));

    sph_bmw512_init(&ctx_bmw);
    sph_bmw512 (&ctx_bmw, static_cast<const void*>(&hash[0]), 64);
    sph_bmw512_close(&ctx_bmw, static_cast<void*>(&hash[1]));

    sph_groestl512_init(&ctx_groestl);
    sph_groestl512 (&ctx_groestl, static_cast<const void*>(&hash[1]), 64);
    sph_groestl512_close(&ctx_groestl, static_cast<void*>(&hash[2]));


    sph_skein512_init(&ctx_skein);
    sph_skein512 (&ctx_skein, static_cast<const void*>(&hash[2]), 64);
    sph_skein512_close(&ctx_skein, static_cast<void*>(&hash[3]));

    sph_jh512_init(&ctx_jh);
    sph_jh512 (&ctx_jh, static_cast<const void*>(&hash[3]), 64);
    sph_jh512_close(&ctx_jh, static_cast<void*>(&hash[4]));

    sph_blake512_init(&ctx_blake);
    sph_blake512 (&ctx_blake, (pbegin == pend ? pblank : static_cast<const void*>(&pbegin[5])), (pend - pbegin) * sizeof(pbegin[5]));
    sph_blake512_close(&ctx_blake, static_cast<void*>(&hash[6]));

    sph_groestl512_init(&ctx_groestl);
    sph_groestl512 (&ctx_groestl, static_cast<const void*>(&hash[6]), 64);
    sph_groestl512_close(&ctx_groestl, static_cast<void*>(&hash[7]));


    sph_skein512_init(&ctx_skein);
    sph_skein512 (&ctx_skein, static_cast<const void*>(&hash[7]), 64);
    sph_skein512_close(&ctx_skein, static_cast<void*>(&hash[8]));

    sph_jh512_init(&ctx_jh);
    sph_jh512 (&ctx_jh, static_cast<const void*>(&hash[8]), 64);
    sph_jh512_close(&ctx_jh, static_cast<void*>(&hash[9]));

     return hash[9].trim256();

}
