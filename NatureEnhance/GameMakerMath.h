#pragma once

#include <cmath>

#define sqr(x) ((x) * (x))

constexpr double PI = 3.1415926535897931;

double clamp(double val, double valmin, double valmax);
double lengthdir_x(double len, double dir);
double lengthdir_y(double len, double dir);
double point_direction(double x1, double y1, double x2, double y2);
double point_distance(double x1, double y1, double x2, double y2);
double sign(double x);

char* toUpperAscii(const char* str);