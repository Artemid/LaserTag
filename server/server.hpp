#ifndef SERVER_H
#define SERVER_H

#include <map>
#include <boost/asio.hpp>

#include "session.hpp"

class LaserTagServer {
    public:
        LaserTagServer(boost::asio::io_service &io_service, short port); 

    private:
        void Receive();
        void onReceive(const boost::system::error_code &error, size_t bytes_transferred, std::shared_ptr<boost::asio::ip::udp::endpoint> client_endpoint,
                std::shared_ptr<Protocol::ClientDataHeader> header, std::shared_ptr<Protocol::TransmittedData> data); 
        void NewSession(boost::asio::ip::udp::endpoint &endpoint);
        void Laser(LaserTagClientSession &firing_session);
        void Send(const boost::system::error_code &error);
        std::shared_ptr<std::vector<Protocol::TransmittedData>> GameState();
        void OnSend(const boost::system::error_code &error, size_t bytes_transferred, 
                std::shared_ptr<std::vector<Protocol::TransmittedData>> game_state, std::shared_ptr<Protocol::ServerDataHeader> header);
        boost::asio::ip::udp::socket socket_;
        boost::asio::deadline_timer timer_;
        std::map<int, LaserTagClientSession> client_sessions_;
        int red_team_count_, blue_team_count_, player_count_;
        int red_score_, blue_score_;
        int server_seq_num_;
};

#endif
