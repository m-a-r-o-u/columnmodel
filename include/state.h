#pragma once
#include <cmath>
#include <memory>
#include "linearfield.h"
#include "layer_quantities.h"
#include "level_quantities.h"
#include "grid.h"

struct State {
    double t;
    std::vector<Layer> layers;
    std::vector<Level> levels;
    const Grid& grid;
    double cloud_base;
    double w_init;
    double qr_ground = 0;

    inline Layer& layer_at(double z) {
        int index = std::floor(z / grid.length);
        return layers[index];
    }

    inline void change_layer(double z, const Layer&& tendencies){
        Layer& l = layer_at(z);
        l += tendencies;
    }
    inline const Level& lower_level_at(double z) {
        int index = std::floor(z / grid.length);
        return levels[index];
    }
    inline const Level& upper_level_at(double z) {
        int index = std::ceil(z / grid.length);
        return levels[index];
    }
};
