#ifndef WEBRTC_VAD_VAD_GMM_HPP
#define WEBRTC_VAD_VAD_GMM_HPP
#include "webrtc/common.hpp"
#include "webrtc/singal_processing/singal_processing_library.hpp"

namespace webrtc {
// Calculates the probability for |input|, given that |input| comes from a
// normal distribution with mean and standard deviation (|mean|, |std|).
//
// Inputs:
//      - input         : input sample in Q4.
//      - mean          : mean input in the statistical model, Q7.
//      - std           : standard deviation, Q7.
//
// Output:
//
//      - delta         : input used when updating the model, Q11.
//                        |delta| = (|input| - |mean|) / |std|^2.
//
// Return:
//   (probability for |input|) =
//    1 / |std| * exp(-(|input| - |mean|)^2 / (2 * |std|^2));
int32_t WebRtcVad_GaussianProbability(int16_t input, int16_t mean, int16_t std, int16_t* delta);

static const int32_t kCompVar = 22005;
static const int16_t kLog2Exp = 5909;  // log2(exp(1)) in Q12.

// For a normal distribution, the probability of |input| is calculated and
// returned (in Q20). The formula for normal distributed probability is
//
// 1 / s * exp(-(x - m)^2 / (2 * s^2))
//
// where the parameters are given in the following Q domains:
// m = |mean| (Q7)
// s = |std| (Q7)
// x = |input| (Q4)
// in addition to the probability we output |delta| (in Q11) used when updating
// the noise/speech model.
inline int32_t WebRtcVad_GaussianProbability(int16_t input, int16_t mean, int16_t std, int16_t* delta) {
    int16_t tmp16, inv_std, inv_std2, exp_value = 0;
    int32_t tmp32;

    // Calculate |inv_std| = 1 / s, in Q10.
    // 131072 = 1 in Q17, and (|std| >> 1) is for rounding instead of truncation.
    // Q-domain: Q17 / Q7 = Q10.
    tmp32 = (int32_t)131072 + (int32_t)(std >> 1);
    inv_std = (int16_t)WebRtcSpl_DivW32W16(tmp32, std);

    // Calculate |inv_std2| = 1 / s^2, in Q14.
    tmp16 = (inv_std >> 2);  // Q10 -> Q8.
    // Q-domain: (Q8 * Q8) >> 2 = Q14.
    inv_std2 = (int16_t)((tmp16 * tmp16) >> 2);
    // TODO(bjornv): Investigate if changing to
    // inv_std2 = (int16_t)((inv_std * inv_std) >> 6);
    // gives better accuracy.

    tmp16 = (input << 3);  // Q4 -> Q7
    tmp16 = tmp16 - mean;  // Q7 - Q7 = Q7

    // To be used later, when updating noise/speech model.
    // |delta| = (x - m) / s^2, in Q11.
    // Q-domain: (Q14 * Q7) >> 10 = Q11.
    *delta = (int16_t)((inv_std2 * tmp16) >> 10);

    // Calculate the exponent |tmp32| = (x - m)^2 / (2 * s^2), in Q10. Replacing
    // division by two with one shift.
    // Q-domain: (Q11 * Q7) >> 8 = Q10.
    tmp32 = (*delta * tmp16) >> 9;

    // If the exponent is small enough to give a non-zero probability we calculate
    // |exp_value| ~= exp(-(x - m)^2 / (2 * s^2))
    //             ~= exp2(-log2(exp(1)) * |tmp32|).
    if (tmp32 < kCompVar) {
        // Calculate |tmp16| = log2(exp(1)) * |tmp32|, in Q10.
        // Q-domain: (Q12 * Q10) >> 12 = Q10.
        tmp16 = (int16_t)((kLog2Exp * tmp32) >> 12);
        tmp16 = -tmp16;
        exp_value = (0x0400 | (tmp16 & 0x03FF));
        tmp16 ^= 0xFFFF;
        tmp16 >>= 10;
        tmp16 += 1;
        // Get |exp_value| = exp(-|tmp32|) in Q10.
        exp_value >>= tmp16;
    }

    // Calculate and return (1 / s) * exp(-(x - m)^2 / (2 * s^2)), in Q20.
    // Q-domain: Q10 * Q10 = Q20.
    return inv_std * exp_value;
}
}  // namespace webrtc
#endif