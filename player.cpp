#include "geometry.cpp"
#include "protocol.hpp"

using namespace CommProtocol;
using namespace Geometry;

class Player {
    public:
        Player(const TransmittedData &data) 
            : player_num_(data.player_num),
              position_(data.x_pos, data.y_pos),
              direction_(data.dir_x, data.dir_y),
              team_(data.team),
              shooting_(data.shooting) {}

        TransmittedData Data() {
            TransmittedData data;
            data.player_num = player_num_;
            data.x_pos = position_.x;
            data.y_pos = position_.y;
            data.dir_x = direction_.x;
            data.dir_y = direction_.y;
            data.team = team_;
            data.shooting = shooting_;
            
            return data;
        }

        void Update(const TransmittedData &data) {
            player_num_ = data.player_num;
            team_ = data.team;
            position_ = Vector2D(data.x_pos, data.y_pos);
            direction_ = Vector2D(data.dir_x, data.dir_y);
            shooting_ = data.shooting;
        }

        std::vector<Vector2D> Vertices() {
            Vector2D nose(position_ + direction_ * 10);
            Vector2D l_wing(position_ + Vector2D(-direction_.y, direction_.x) * 5);
            Vector2D r_wing(position_ + Vector2D(direction_.y, -direction_.x) * 5);
            std::vector<Vector2D> vertices = {nose, l_wing, r_wing};
            
            return vertices;
        }

        void MoveForward() {
            position_ = position_ + direction_;
        }

        void MoveBackward() {
            position_ = position_ - direction_;
        }

        void RotateRight() {
            direction_ = RotateDegrees(direction_, -5);
        }

        void RotateLeft() {
            direction_ = RotateDegrees(direction_, 5);
        }

        void SetShooting(bool shooting) {
            shooting_ = shooting;    
        }

        int PlayerNum() {
            return player_num_;
        }

        Team Team() {
            return team_;
        }

        const Vector2D &Position() {
            return position_;
        }

        const Vector2D &Direction() {
            return direction_;
        }

        bool Shooting() {
            return shooting_;
        }

    private:
        int player_num_;
        ::Team team_;
        Vector2D position_;
        Vector2D direction_;
        int shooting_;
};
