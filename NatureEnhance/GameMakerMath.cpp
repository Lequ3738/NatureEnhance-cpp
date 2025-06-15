#include "GameMakerMath.h"

double clamp(double val, double valmin, double valmax)
{
    return fmax(valmin, fmin(val, valmax));
}

double lengthdir_x(double len, double dir)
{
    double v = len * cos(dir * PI / 180);
    double flv = floor(v);

    if (abs(v - flv) < 0.0001)
        return flv;
    else
        return v;
}

double lengthdir_y(double len, double dir)
{
    double v = len * sin(dir * PI / 180) * -1;
    double flv = floor(v);

    if (abs(v - flv) < 0.0001)
        return flv;
    else
        return v;
}

double point_direction(double x1, double y1, double x2, double y2)
{
    double x = x2 - x1;
    double y = y2 - y1;

    if (x == 0)
    {
        if (y > 0)
            return 270;
        else if (y < 0)
            return 90;
        else
            return 0;
    }
    else
    {
        double d = 180 * atan2(y, x) / PI;

        return d <= 0 ? -d : 360 - d;
    }
}

double point_distance(double x1, double y1, double x2, double y2)
{
    return abs(sqrt(sqr(x2 - x1) + sqr(y2 - y1)));
}

double sign(double x) { return (x > 0) - (x < 0); }

char* toUpperAscii(const char* str)  
{  
    if (str == nullptr)  
        return nullptr;  

    size_t len = strlen(str) + 1;  
    char* copy = new char[len];  
    strcpy_s(copy, len, str);  

    for (size_t i = 0; i < len - 1; i++)
    {  
        if (copy[i] >= 'a' && copy[i] <= 'z')  
            copy[i] -= 32;  
    }  

    return copy;  
}