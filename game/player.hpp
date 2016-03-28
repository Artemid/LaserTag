#ifndef PLAYER_H
#define PLAYER_H

#include "geometry.hpp"
#include "protocol.hpp"

class Player {
    public:
        Player(const Protocol::TransmittedData &data); 

        Protocol::TransmittedData Data();

        void Update(const Protocol::TransmittedData &data);

        std::vector<Geometry::Vector2D> Vertices() const;

        void MoveForward();

        void MoveBackward();

        void RotateRight();

        void RotateLeft();

        void SetLaser(bool laser);

        void SetPosition(const Geometry::Vector2D &pos);

        void SetDirection(const Geometry::Vector2D &dir);

        int PlayerNum() const;

        Protocol::Team Team() const;

        const Geometry::Vector2D &Position() const;

        const Geometry::Vector2D &Direction() const;

        bool Laser() const;

    private:
        int player_num_;
        Protocol::Team team_;
        Geometry::Vector2D position_;
        Geometry::Vector2D direction_;
        int laser_;
};

#endif
