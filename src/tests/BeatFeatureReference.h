// Generated independently with beat_this.preprocessing.LogMelSpect (Torch).
#ifndef DSSP_TESTS_BEATFEATUREREFERENCE_H
#define DSSP_TESTS_BEATFEATUREREFERENCE_H
#include <array>
#include <cmath>
#include <utility>
#include <vector>
namespace dssp::tests {
inline std::vector<float> beatReferenceAudio() {
    std::vector<float> audio(2049);
    for (int i = 0; i < 2049; ++i) audio[i] = static_cast<float>(std::sin(i * 0.043) * 0.2 + ((i * 17) % 31 - 15) * 0.003);
    return audio;
}
inline constexpr std::array BeatReferenceProbes{
    std::pair<std::size_t, float>{0, 5.99769115f},
    std::pair<std::size_t, float>{1, 6.16931152f},
    std::pair<std::size_t, float>{5, 6.81024981f},
    std::pair<std::size_t, float>{10, 4.68036175f},
    std::pair<std::size_t, float>{20, 3.37866735f},
    std::pair<std::size_t, float>{40, 2.20735931f},
    std::pair<std::size_t, float>{70, 2.70338511f},
    std::pair<std::size_t, float>{100, 2.73845029f},
    std::pair<std::size_t, float>{126, 4.96771717f},
    std::pair<std::size_t, float>{127, 4.80375147f},
    std::pair<std::size_t, float>{128, 2.29635358f},
    std::pair<std::size_t, float>{129, 2.30972576f},
    std::pair<std::size_t, float>{133, 6.14775515f},
    std::pair<std::size_t, float>{138, 1.75478816f},
    std::pair<std::size_t, float>{148, 0.810038924f},
    std::pair<std::size_t, float>{168, 0.323958755f},
    std::pair<std::size_t, float>{198, 0.543037832f},
    std::pair<std::size_t, float>{228, 0.50806576f},
    std::pair<std::size_t, float>{254, 2.76169801f},
    std::pair<std::size_t, float>{255, 3.9557457f},
    std::pair<std::size_t, float>{256, 0.160220668f},
    std::pair<std::size_t, float>{257, 0.394052476f},
    std::pair<std::size_t, float>{261, 6.13819933f},
    std::pair<std::size_t, float>{266, 0.0375468843f},
    std::pair<std::size_t, float>{276, 0.00231731473f},
    std::pair<std::size_t, float>{296, 0.00050908362f},
    std::pair<std::size_t, float>{326, 0.0205945149f},
    std::pair<std::size_t, float>{356, 0.0174127035f},
    std::pair<std::size_t, float>{382, 2.38657928f},
    std::pair<std::size_t, float>{383, 3.93341017f},
    std::pair<std::size_t, float>{384, 0.162176684f},
    std::pair<std::size_t, float>{385, 0.395059079f},
    std::pair<std::size_t, float>{389, 6.13819933f},
    std::pair<std::size_t, float>{394, 0.0375741832f},
    std::pair<std::size_t, float>{404, 0.00500447536f},
    std::pair<std::size_t, float>{424, 0.000953966402f},
    std::pair<std::size_t, float>{454, 0.0219290797f},
    std::pair<std::size_t, float>{484, 0.0264174752f},
    std::pair<std::size_t, float>{510, 2.38098741f},
    std::pair<std::size_t, float>{511, 3.93342638f},
    std::pair<std::size_t, float>{512, 5.13798189f},
    std::pair<std::size_t, float>{513, 5.30338144f},
    std::pair<std::size_t, float>{517, 6.42651749f},
    std::pair<std::size_t, float>{522, 3.77925181f},
    std::pair<std::size_t, float>{532, 2.41957879f},
    std::pair<std::size_t, float>{552, 1.15018499f},
    std::pair<std::size_t, float>{582, 0.546354532f},
    std::pair<std::size_t, float>{612, 2.2457099f},
    std::pair<std::size_t, float>{638, 3.86541176f},
    std::pair<std::size_t, float>{639, 4.19573212f}
};
}
#endif
