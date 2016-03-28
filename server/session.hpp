#include <boost/asio.hpp>
#include <boost/random.hpp>

#include "player.hpp"
#include "geometry.hpp"

class LaserTagClientSession {
    public:
        LaserTagClientSession(boost::asio::ip::udp::endpoint client_endpoint, Protocol::TransmittedData &data); 

        Protocol::TransmittedData ClientState();

        const Player &GetPlayer();

        void UpdateClientState(int new_seq_num, Protocol::TransmittedData &data);

        const boost::asio::ip::udp::endpoint &GetEndpoint();

        bool SessionExpired();

        void Spawn();

    private:
        boost::asio::ip::udp::endpoint endpoint_;
        boost::posix_time::ptime last_received_;
        unsigned int seq_num_;
        boost::mt19937 random_num_gen_; 
        Player player_;
};
