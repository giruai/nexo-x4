/* SPDX-License-Identifier: GPL-3.0-only */
#include "../src/media/decode_recovery_policy.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    assert(x4_decode_recovery_matches(X4_SPS_CHANGED, "SPS H264 CAMBIADO"));
    assert(x4_decode_recovery_matches((int)X4_DECODE_OBSERVED_ERROR, "DECODIFICAR JUEGO H264"));
    assert(!x4_decode_recovery_matches(X4_SPS_INVALID, "SPS H264 CAMBIADO"));
    assert(!x4_decode_recovery_matches(X4_SPS_LEVEL_DPB, "SPS H264 INVALIDO"));
    assert(!x4_decode_recovery_matches(X4_SPS_SIZE, "TIMEOUT RESOLUCION INICIAL"));
    assert(!x4_decode_recovery_matches(X4_SPS_CHANGED, "CREAR DECODER"));
    assert(!x4_decode_recovery_matches(X4_SPS_CHANGED, NULL));
    X4DecodeRecoveryPolicy p = {0};
    for (unsigned i=0;i<X4_DECODE_RECOVERY_MAX;++i) assert(x4_decode_recovery_allow(&p, 1000000+i));
    assert(!x4_decode_recovery_allow(&p, 29999999));
    assert(x4_decode_recovery_allow(&p, 31000000));
    assert(p.attempts==1);
    puts("decode recovery: stage gating, SPS admission and bounded restart budget passed");
}
