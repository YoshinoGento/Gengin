#pragma once
#include <array>
#include <istream>

namespace canal_lab {
inline constexpr int Width=11, Height=7, Count=Width*Height;
struct Point { int x{},y{}; bool operator==(const Point&) const = default; };
enum Side { North, East, South, West };
inline constexpr std::array<Point,4> Offset{{{0,-1},{1,0},{0,1},{-1,0}}};
inline constexpr Point Next(Point p,int d) { return {p.x+Offset[d].x,p.y+Offset[d].y}; }
inline constexpr bool Inside(Point p) { return p.x>=0 && p.y>=0 && p.x<Width && p.y<Height; }
inline constexpr int Index(Point p) { return p.y*Width+p.x; }
inline constexpr int Opposite(int d) { return (d+2)%4; }
inline constexpr bool InPondRing(Point p) { return p.x>=-1 && p.y>=-1 && p.x<=Width && p.y<=Height && !Inside(p); }
enum class Tool { Dig, Bank };
struct Cell {
    char terrain='.';
    bool dug=false;
    unsigned mouths=0;
    int wall=-1;
    float volume=0;
    float currentX=0,currentY=0;
};
struct Balance { double admitted=0, seeped=0; };

class System {
public:
    bool Load(std::istream& map);
    void Reset() noexcept;
    bool Stroke(Point a,Point b,Tool tool) noexcept;
    void Step(float seconds) noexcept;
    const Cell* At(Point p) const noexcept { return Inside(p)?&cells_[Index(p)]:nullptr; }
    int Arms(Point p) const noexcept;
    const Balance& Budget() const noexcept { return balance_; }
    float Delivered() const noexcept { return basin_>=0?cells_[basin_].volume:0; }
    float LeakRate() const noexcept {return leakRate_;}
    static constexpr float FullBasin=1.55f;
private:
    bool Edge(Point a,Point b,int d,Tool tool) noexcept;
    bool Dig(Point p) noexcept;
    bool Linked(Point a,int d) const noexcept;
    void Flow(float dt) noexcept;
    std::array<Cell,Count> cells_{};
    int basin_=-1;
    Balance balance_{};
    float leakRate_=0;
};
}
