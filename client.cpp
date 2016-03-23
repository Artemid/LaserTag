#include <iostream>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

struct TransmittedData {
    bool init; // True if player is entering game
    int player_num;
    float x_pos;
    float y_pos;
    float direction;
};

class TeamBattleClient {
    public:
        TeamBattleClient(boost::asio::io_service &io_service, boost::asio::ip::udp::endpoint endpoint) 
            : socket_(io_service, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0)), 
              endpoint_(endpoint),
              timeout_timer_(io_service) {
            RequestEnterGame();
        }

        void RequestEnterGame() {
            TransmittedData request_enter_game;
            request_enter_game.init = true;
            std::shared_ptr<std::vector<TransmittedData>> request(new std::vector<TransmittedData>());
            request->push_back(request_enter_game);
            socket_.async_send_to(boost::asio::buffer(*request), endpoint_, boost::bind(&TeamBattleClient::OnRequestEnterGame, this, _1, _2, request));
        }

        void OnRequestEnterGame(const boost::system::error_code &error, size_t bytes_transferred, std::shared_ptr<std::vector<TransmittedData>> request) {
            ReceiveGameData();
            timeout_timer_.expires_from_now(boost::posix_time::seconds(1));
            timeout_timer_.async_wait(boost::bind(&TeamBattleClient::OnEnterGameTimeout, this, _1));
        }

        void OnEnterGameTimeout(const boost::system::error_code &error) {
            if (error) {
                // Timer was cancelled i.e. game data was received
                return;
            }
            RequestEnterGame();
        }

        void ReceiveGameData() {
            std::shared_ptr<std::vector<TransmittedData>> transmitted_data(new std::vector<TransmittedData>(10));
            socket_.async_receive_from(boost::asio::buffer(*transmitted_data), endpoint_, boost::bind(&TeamBattleClient::OnReceiveGameData, this, _1, _2, transmitted_data));
        }

        void OnReceiveGameData(const boost::system::error_code &error, size_t bytes_transmitted, std::shared_ptr<std::vector<TransmittedData>> transmitted_data) {
            timeout_timer_.cancel();
            std::cout << "Received " << transmitted_data->size() << " player_nums: ";
            for (TransmittedData received_data : *transmitted_data) {
                std::cout << received_data.player_num << " ";
            }
            std::cout << std::endl;
            ReceiveGameData();
        }

    private:
        boost::asio::ip::udp::socket socket_;
        boost::asio::ip::udp::endpoint endpoint_;
        boost::asio::deadline_timer timeout_timer_;

        int player_num_;
        float x_pos_;
        float y_pos_;
        float direction_;
};

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
            TeamBattleClient client(io_service, endpoint);
            io_service.run();
        }
    } catch (std::exception exc) {
        std::cerr << "Execption: " << exc.what() << std::endl;
    }
}
