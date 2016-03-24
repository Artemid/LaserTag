#include <iostream>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

struct TransmittedDataHeader {
    int client_player_num;
    int num_players;
};

struct TransmittedData {
    int init; // True if player is entering game
    int player_num;
    int x_pos;
    int y_pos;
    int direction;
    int seq_num;
};

class TeamBattleClientSession {
    public:
        TeamBattleClientSession(boost::asio::io_service &io_service, boost::asio::ip::udp::endpoint endpoint) 
            : socket_(io_service, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0)), 
              endpoint_(endpoint),
              timeout_timer_(io_service),
              send_timer_(io_service),
              entered_game_(false) {
            // Send initial packet to server
            RequestEnterGame();
        }

        void RequestEnterGame() {
            TransmittedData request_enter_game;
            request_enter_game.init = true;
            std::shared_ptr<TransmittedData> request(new TransmittedData(request_enter_game));
            socket_.async_send_to(boost::asio::buffer(request.get(), sizeof(TransmittedData)), endpoint_, boost::bind(&TeamBattleClientSession::OnRequestEnterGame, this, _1, _2, request));
        }

        void OnRequestEnterGame(const boost::system::error_code &error, size_t bytes_transferred, std::shared_ptr<TransmittedData> request) {
            ReceiveGameData();
            timeout_timer_.expires_from_now(boost::posix_time::seconds(1));
            timeout_timer_.async_wait(boost::bind(&TeamBattleClientSession::OnEnterGameTimeout, this, _1));
        }

        void OnEnterGameTimeout(const boost::system::error_code &error) {
            if (error) {
                // Timer was cancelled i.e. game data was received
                return;
            }
            RequestEnterGame();
        }

        void ReceiveGameData() {
            // Receive game data in form of header data and vector of player states
            std::shared_ptr<TransmittedDataHeader> header(new TransmittedDataHeader());
            std::shared_ptr<std::vector<TransmittedData>> data(new std::vector<TransmittedData>(32));
            boost::array<boost::asio::mutable_buffer, 2> buffer = {boost::asio::buffer(header.get(), sizeof(TransmittedDataHeader)), boost::asio::buffer(*data)};
            socket_.async_receive_from(buffer, endpoint_, boost::bind(&TeamBattleClientSession::OnReceiveGameData, this, _1, _2, header, data));
        }

        void OnReceiveGameData(const boost::system::error_code &error, size_t bytes_transmitted, 
                std::shared_ptr<TransmittedDataHeader> transmitted_data_header, std::shared_ptr<std::vector<TransmittedData>> transmitted_data) {
            // Cancel timer after we receive game data
            timeout_timer_.cancel();

            // Fetch data from buffer
            transmitted_data->resize(transmitted_data_header->num_players);
            TransmittedData my_data;
            std::cout << "Player " << transmitted_data_header->client_player_num << std::endl;
            for (TransmittedData received_data : *transmitted_data) {
                std::cout << "\t{" << received_data.init 
                    << "," << received_data.player_num 
                    << "," << received_data.x_pos 
                    << "," << received_data.y_pos 
                    << "," << received_data.direction 
                    << "}" << std::endl;
                
                if (received_data.player_num == player_num_) {
                    my_data = received_data;
                }
            }
            
            // If this is the first packet
            if (!entered_game_) {
                // Save player id and initial coordinates
                player_num_ = transmitted_data_header->client_player_num;
                x_pos_ = my_data.x_pos;
                y_pos_ = my_data.y_pos;
                direction_ = my_data.direction;

                // Begin sending current data
                send_timer_.expires_from_now(boost::posix_time::milliseconds(200));
                send_timer_.async_wait(boost::bind(&TeamBattleClientSession::SendPlayerData, this, _1));
            
                // Set flag
                entered_game_ = true;
            }

            // Receive next
            ReceiveGameData();
        }

        void SendPlayerData(const boost::system::error_code &error) {
            // Create packet
            std::shared_ptr<TransmittedData> curr_data(new TransmittedData());
            curr_data->init = false;
            curr_data->player_num = player_num_;
            curr_data->x_pos = x_pos_++ % 100;
            curr_data->y_pos = y_pos_++ % 100;
            curr_data->direction = direction_++ % 360;
            
            // Send asynchronously
            socket_.async_send_to(boost::asio::buffer(curr_data.get(), sizeof(TransmittedData)), endpoint_, boost::bind(&TeamBattleClientSession::OnSendPlayerData, this, _1, _2, curr_data));
        }

        void OnSendPlayerData(const boost::system::error_code &error, size_t bytes_transmitted, std::shared_ptr<TransmittedData> data) {
            // Timer for the next send
            send_timer_.expires_from_now(boost::posix_time::milliseconds(200));
            send_timer_.async_wait(boost::bind(&TeamBattleClientSession::SendPlayerData, this, _1));
        }

    private:
        boost::asio::ip::udp::socket socket_;
        boost::asio::ip::udp::endpoint endpoint_;
        boost::asio::deadline_timer timeout_timer_;
        boost::asio::deadline_timer send_timer_;

        bool entered_game_;
        int player_num_;
        int x_pos_;
        int y_pos_;
        int direction_;
};

class TeamBattleRenderer {

}

class TeamBattleClient {

}

int main(int argc, char **argv) {
    try {
        if (argc < 3) {
            std::cerr << "Usage: TeamBattleClient <remote_address> <remote_port>" << std::endl;
            return -1;
        } else {
            boost::asio::io_service io_service;
            boost::asio::ip::udp::resolver resolver(io_service);
            boost::asio::ip::udp::resolver::query query(boost::asio::ip::udp::v4(), argv[1], argv[2]);
            boost::asio::ip::udp::endpoint endpoint = *resolver.resolve(query);
            TeamBattleClientSession client(io_service, endpoint);
            io_service.run();
        }
    } catch (std::exception exc) {
        std::cerr << "Execption: " << exc.what() << std::endl;
    }
}
