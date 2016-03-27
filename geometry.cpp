#include <vector>
#include <cmath>
#include <limits>

namespace Geometry {

class Vector2D {
    public:
        Vector2D(float x_, float y_) : x(x_), y(y_) {}
        
        Vector2D operator*(float scalar) const { 
            return Vector2D(scalar * x, scalar * y); 
        }
        
        float x;
        float y;
};

Vector2D operator+(const Vector2D &lhs, const Vector2D &rhs) {
    return Vector2D(lhs.x + rhs.x, lhs.y + rhs.y);
}

Vector2D operator-(const Vector2D &lhs, const Vector2D &rhs) {
    return Vector2D(lhs.x - rhs.x, lhs.y - rhs.y);
}

Vector2D RotateRadians(const Vector2D &vec, float radians) {
    float cos = cosf(radians);
    float sin = sinf(radians);
    float x_new = vec.x * cos - vec.y * sin;
    float y_new = vec.x * sin + vec.y * cos;
    return Vector2D(x_new, y_new);
}

Vector2D RotateDegrees(const Vector2D &vec, float degrees) {
    float radians = degrees * 4.0 * atan(1.0) / 180.0;
    return RotateRadians(vec, radians);
}

float Dot(const Vector2D &lhs, const Vector2D &rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y;
}

float Norm(const Vector2D &vec) {
    return sqrtf(vec.x * vec.x + vec.y * vec.y);    
}

// NOTE poly_verts must be vertices of CONVEX polygon in ANTICLOCKWISE ORDER
bool VectorIntersectsConvexPolygon(const std::vector<Vector2D> &poly_verts, const Vector2D &point, const Vector2D &direction) {
    float t_near = 0.0;
    float t_far = std::numeric_limits<float>::max();

    for (int i = 0, j = poly_verts.size() - 1; i < poly_verts.size(); j = i, i++) {
        const Vector2D &e0 = poly_verts[j];
        const Vector2D &e1 = poly_verts[i];
        Vector2D e = e1 - e0;
        Vector2D e_normal = Vector2D(e.y, -e.x);
        Vector2D d = e0 - point;
        float numer = Dot(d, e_normal);
        float denom = Dot(direction, e_normal);

        float t_clip = numer / denom;
        if (denom < 0.0) {
            if (t_clip > t_far)
                return false;
            if (t_clip > t_near)
                t_near = t_clip;
        } else {
            if (t_clip < t_near)
                return false;
            if (t_clip < t_far)
                t_far = t_clip;
        }
    }

    return true;
}

}
