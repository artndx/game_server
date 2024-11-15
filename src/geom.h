#pragma once

#include <tuple>

namespace model {

using Dimension = int;
using Coord = Dimension;

struct Point {
    Coord x, y;
};

struct Size {
    Dimension width, height;
};

enum class Direction{
    NORTH,
    SOUTH,
    WEST,
    EAST
};

struct Rectangle {
    Point position;
    Size size;
};

struct Offset {
    Dimension dx, dy;
};

struct PairDouble{
    double x = 0;
    double y = 0;
};

inline bool operator==(const PairDouble& lhs, const PairDouble& rhs){
    return std::tie(lhs.x, lhs.y) == std::tie(rhs.x, rhs.y);
}

inline bool operator!=(const PairDouble& lhs, const PairDouble& rhs){
    return std::tie(lhs.x, lhs.y) != std::tie(rhs.x, rhs.y);
}

} // namespace model