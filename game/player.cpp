#include "protocol.hpp"
#include "geometry.hpp"

#include "player.hpp"

using namespace Protocol;
using namespace Geometry;

Player::Player(const TransmittedData &data) 
    : player_num_(data.player_num),
      position_(data.x_pos, data.y_pos),
      direction_(data.dir_x, data.dir_y),
      team_(data.team),
      laser_(data.laser) {}


TransmittedData Player::Data() {
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


void Player::Update(const TransmittedData &data) {
    player_num_ = data.player_num;
    team_ = data.team;
    position_ = Vector2D(data.x_pos, data.y_pos);
    direction_ = Vector2D(data.dir_x, data.dir_y);
    laser_ = data.laser;
}


std::vector<Vector2D> Player::Vertices() const {
    Vector2D nose(position_ + direction_ * 10);
    Vector2D l_wing(position_ + Vector2D(-direction_.y, direction_.x) * 5);
    Vector2D r_wing(position_ + Vector2D(direction_.y, -direction_.x) * 5);
    std::vector<Vector2D> vertices = {nose, l_wing, r_wing};
    
    return vertices;
}


void Player::MoveForward() {
    position_ = position_ + direction_;
}

void Player::MoveBackward() {
    position_ = position_ - direction_;
}

void Player::RotateRight() {
    direction_ = RotateDegrees(direction_, -5);
}

void Player::RotateLeft() {
    direction_ = RotateDegrees(direction_, 5);
}

void Player::SetLaser(bool laser) {
    laser_ = laser;    
}

void Player::SetPosition(const Vector2D &pos) {
    position_ = pos;
}

void Player::SetDirection(const Vector2D &dir) {
    direction_ = dir;
}

int Player::PlayerNum() const {
    return player_num_;
}

Team Player::Team() const {
    return team_;
}

const Vector2D &Player::Position() const {
    return position_;
}

const Vector2D &Player::Direction() const {
    return direction_;
}

bool Player::Laser() const {
    return laser_;
}
