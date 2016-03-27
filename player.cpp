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
              laser_(data.laser) {}

        TransmittedData Data() {
            TransmittedData data;
            data.player_num = player_num_;
            data.x_pos = position_.x;
            data.y_pos = position_.y;
            data.dir_x = direction_.x;
            data.dir_y = direction_.y;
            data.team = team_;
            data.laser = laser_;
            
            return data;
        }

        void Update(const TransmittedData &data) {
            player_num_ = data.player_num;
            team_ = data.team;
            position_ = Vector2D(data.x_pos, data.y_pos);
            direction_ = Vector2D(data.dir_x, data.dir_y);
            laser_ = data.laser;
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

        void SetLaser(bool laser) {
            laser_ = laser;    
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

        bool Laser() {
            return laser_;
        }

    private:
        int player_num_;
        ::Team team_;
        Vector2D position_;
        Vector2D direction_;
        int laser_;
};
