#ifndef GEOMETRY_H
#define GEOMETRY_H

#include <vector>
#include <cmath>
#include <limits>

namespace Geometry {

class Vector2D {
    public:
        Vector2D(float x_, float y_);
        
        float x;
        float y;
};

Vector2D operator+(const Vector2D &lhs, const Vector2D &rhs);

Vector2D operator-(const Vector2D &lhs, const Vector2D &rhs);

Vector2D operator*(const Vector2D &lhs, const float scalar);

Vector2D RotateRadians(const Vector2D &vec, float radians);

Vector2D RotateDegrees(const Vector2D &vec, float degrees);

float Dot(const Vector2D &lhs, const Vector2D &rhs);

float Norm(const Vector2D &vec);

bool VectorIntersectsConvexPolygon(const std::vector<Vector2D> &poly_verts, const Vector2D &point, const Vector2D &direction);

}

#endif
