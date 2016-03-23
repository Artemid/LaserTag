#include <iostream>
#include <map>
#include <vector>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

struct TransmittedDataHeader {
    int num_players;
    int client_player_num;
};

struct TransmittedData {
    bool init; // True if player is entering game
    int player_num;
    float x_pos;
    float y_pos;
    float direction;
};

class TeamBattleClientSession {
    public:
        TeamBattleClientSession(boost::asio::ip::udp::endpoint client_endpoint, int player_num) : endpoint_(client_endpoint), player_num_(player_num) {
            x_pos_ = 0;
            y_pos_ = 0;
            direction_ = 0;
        }

        TransmittedData GetClientState() {
            TransmittedData data;
            data.player_num = player_num_;
            data.x_pos = x_pos_;
            data.y_pos = y_pos_;
            data.direction = direction_;
            return data;
        }

        boost::asio::ip::udp::endpoint &GetEndpoint() {
            return endpoint_;
        }

    private:
        boost::asio::ip::udp::endpoint endpoint_;
        int player_num_;
        float x_pos_;
        float y_pos_;
        float direction_;
};

class TeamBattleServer {
    public:
        TeamBattleServer(boost::asio::io_service &io_service, short port) 
                : socket_(io_service, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), port)), 
                  timer_(io_service), 
                  player_count_(0) {
            // Begin sending game state to clients
            timer_.expires_from_now(boost::posix_time::milliseconds(200));
            timer_.async_wait(boost::bind(&TeamBattleServer::Send, this, _1));
            
            // Begin receving data from clients
            Receive();
        }

        void Receive() {
            // Initialize data structures to be filled by async receive
            std::shared_ptr<boost::asio::ip::udp::endpoint> client_endpoint(new boost::asio::ip::udp::endpoint());
            std::shared_ptr<TransmittedData> player_data(new TransmittedData());
            
            // Perform asynchronous read call
            socket_.async_receive_from(boost::asio::buffer(player_data.get(), sizeof(TransmittedData)), *client_endpoint, 
                boost::bind(&TeamBattleServer::onReceive, this, _1, _2, player_data, client_endpoint));
        }

        void onReceive(const boost::system::error_code &error, 
                size_t bytes_transferred, 
                std::shared_ptr<TransmittedData> client_data, 
                std::shared_ptr<boost::asio::ip::udp::endpoint> client_endpoint) { 
            // Process client data from async receive
            if (!error) {
                if (client_data->init) {
                    // Add new client to game
                    TeamBattleClientSession new_session(*client_endpoint, player_count_);
                    client_sessions_.insert(std::pair<int, TeamBattleClientSession>(player_count_, new_session));
                    std::cout << "Added player " << player_count_ << " at " << new_session.GetEndpoint().address() << std::endl;
                    player_count_++;
                } else {
                    // Update state of existing client
                }
            }

            // Receive next client data
            Receive();
        }

        void Send(const boost::system::error_code &error) {
            // Buffer state of game
            std::shared_ptr<std::vector<TransmittedData>> game_state(new std::vector<TransmittedData>());
            for (auto iter = client_sessions_.begin(); iter != client_sessions_.end(); iter++) {
                game_state->push_back(iter->second.GetClientState());         
            }

            // Send state of game to all clients
            for (auto iter = client_sessions_.begin(); iter != client_sessions_.end(); iter++) {
                std::shared_ptr<TransmittedDataHeader> header(new TransmittedDataHeader());
                header->num_players = client_sessions_.size();
                header->client_player_num = iter->second.GetClientState().player_num;
                boost::array<boost::asio::const_buffer, 2> buffer = {boost::asio::buffer(*header), boost::asio::buffer(*game_state)};
                socket_.async_send_to(buffer, iter->second.GetEndpoint(), boost::bind(&TeamBattleServer::OnSend, this, _1, _2, game_state, header));
            }

            // Schedule event to send game state to all clients
            timer_.expires_from_now(boost::posix_time::milliseconds(200));
            timer_.async_wait(boost::bind(&TeamBattleServer::Send, this, _1));
        }

        void OnSend(const boost::system::error_code &error, size_t bytes_transferred, std::shared_ptr<std::vector<TransmittedData>> game_state, std::shared_ptr<TransmittedDataHeader> header) {
            // Method here to maintain ownership of game state data until async send completes
        }

    private:
        boost::asio::ip::udp::socket socket_;
        boost::asio::deadline_timer timer_;

        std::map<int, TeamBattleClientSession> client_sessions_;
        int player_count_;
};

int main(int argc, char **argv) {
    
    try {
        if (argc < 2) {
            std::cerr << "Usage: TeamBattle <port>" << std::endl;
            return -1;
        } else {
            short port = atoi(argv[1]);
            boost::asio::io_service io_service;
            boost::shared_ptr<TeamBattleServer> server(new TeamBattleServer(io_service, port));
            io_service.run();
        }
    } catch (std::exception &exc) {
        std::cerr << "Exception: " << exc.what() << std::endl;
    }

    return 0;
}
