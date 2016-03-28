#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/random.hpp>

#include "player.cpp"
#include "geometry.cpp"

using namespace Geometry;
using namespace Protocol;

class LaserTagClientSession {
    public:
        LaserTagClientSession(boost::asio::ip::udp::endpoint client_endpoint, TransmittedData &data) 
            : endpoint_(client_endpoint), 
              last_received_(boost::posix_time::second_clock::local_time()),
              player_(data),
              seq_num_(0) {
            // Random number generator
            boost::posix_time::ptime time = boost::posix_time::microsec_clock::local_time();
            boost::posix_time::time_duration duration(time.time_of_day());
            random_num_gen_ = boost::mt19937(duration.total_milliseconds());

            // Spawn coordinates and direction
            Spawn();
        }

        TransmittedData ClientState() {
            return player_.Data();
        }

        const Player &GetPlayer() {
            return player_;
        }

        void UpdateClientState(int new_seq_num, TransmittedData &data) {
            if (new_seq_num < seq_num_) {
                // Check sequence number
                return;
            } else if (Norm(player_.Position() - Vector2D(data.x_pos, data.y_pos)) > 5) {
                // Check client didn't try to move too far
                return;
            } else {
                // Update
                last_received_ = boost::posix_time::second_clock::local_time();
                seq_num_ = new_seq_num;
                player_.Update(data);
            }
        }

        const boost::asio::ip::udp::endpoint &GetEndpoint() {
            return endpoint_;
        }

        bool SessionExpired() {
            boost::posix_time::time_duration duration = boost::posix_time::second_clock::local_time() - last_received_;
            return duration.seconds() > 2;
        }

        void Spawn() {
            // Random coordinates in game 
            boost::uniform_real<> coord_distr(-250, 250);
            boost::variate_generator<boost::mt19937 &, boost::uniform_real<>> coord_random(random_num_gen_, coord_distr);
            player_.SetPosition(Vector2D(coord_random(), coord_random()));

            // Random direction
            boost::uniform_int<> dir_distr(0, 71); // 5 degree turns (72 between 0 and 360)
            boost::variate_generator<boost::mt19937 &, boost::uniform_int<>> dir_random(random_num_gen_, dir_distr);
            player_.SetDirection(RotateDegrees(Vector2D(1, 0), dir_random()));
        }

    private:
        // Endpoint of this client session
        boost::asio::ip::udp::endpoint endpoint_;
        
        // Time of last received data, for timeout
        boost::posix_time::ptime last_received_;
        unsigned int seq_num_;

        // Random number generator for respawning
        boost::mt19937 random_num_gen_;
        
        // Data
        Player player_;
};
