#ifndef CLIENT_H
#define CLIENT_H

#include <vector>
#include <mutex>
#include <boost/asio.hpp>

#include "player.hpp"
#include "protocol.hpp"

typedef enum {
    Up = 101,
    Left = 100,
    Right = 102,
    Down = 103,
    Space = 32
} Input;

class LaserTagClient {
    public:
        LaserTagClient(boost::asio::io_service &io_service, std::string hostname, std::string service_id); 

        std::map<int, Player> &Players();

        std::pair<int, int> GetScore();

        int GetPlayerNum();

        void UpdateState(Input input);
    
    private:
        void RequestEnterGame();
        void OnRequestEnterGame(const boost::system::error_code &error, size_t bytes_transferred, std::shared_ptr<Protocol::ClientDataHeader> request);
        void OnEnterGameTimeout(const boost::system::error_code &error);
        void ReceiveGameData(bool initial);
        void OnReceiveInitialGameData(const boost::system::error_code &error, size_t bytes_transmitted, 
                std::shared_ptr<Protocol::ServerDataHeader> transmitted_data_header, std::shared_ptr<std::vector<Protocol::TransmittedData>> transmitted_data);
        void OnReceiveGameData(const boost::system::error_code &error, size_t bytes_transmitted,
                std::shared_ptr<Protocol::ServerDataHeader> transmitted_data_header, std::shared_ptr<std::vector<Protocol::TransmittedData>> transmitted_data);
        void InsertOrUpdatePlayer(int player_num, Protocol::TransmittedData &data);
        void SendPlayerData(const boost::system::error_code &error);
        void OnSendPlayerData(const boost::system::error_code &error, size_t bytes_transmitted, 
                std::shared_ptr<Protocol::ClientDataHeader> header, std::shared_ptr<Protocol::TransmittedData> data);
        void Laser();
        Player &MyPlayer();
        boost::asio::ip::udp::socket socket_;
        boost::asio::ip::udp::endpoint endpoint_;
        boost::asio::deadline_timer timeout_timer_;
        boost::asio::deadline_timer send_timer_;
        boost::asio::deadline_timer laser_timer_;
        boost::asio::deadline_timer laser_available_timer_;
        std::mutex mutex_;
        int my_player_num_;
        std::map<int, Player> players_;
        int red_score_, blue_score_;
        int last_server_seq_num_;
        int seq_num_;
        bool laser_available_;
};

#endif
