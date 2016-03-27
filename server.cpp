#include <iostream>
#include <map>
#include <vector>
#include <cmath>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/random.hpp>

#include "player.cpp"

using namespace CommProtocol;
using namespace Geometry;

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

class LaserTagServer {
    public:
        LaserTagServer(boost::asio::io_service &io_service, short port) 
                : socket_(io_service, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), port)), 
                  timer_(io_service) {
            // Initialize variables
            player_count_ = red_team_count_ = blue_team_count_ = red_score_ = blue_score_ = 0;
            server_seq_num_ = 0;

            // Begin sending game state to clients
            timer_.expires_from_now(boost::posix_time::milliseconds(50));
            timer_.async_wait(boost::bind(&LaserTagServer::Send, this, _1));
            
            // Begin receving data from clients
            Receive();
        }

        void Receive() {
            // Initialize buffer
            std::shared_ptr<ClientDataHeader> header(new ClientDataHeader());
            std::shared_ptr<TransmittedData> data(new TransmittedData());
            boost::array<boost::asio::mutable_buffer, 2> buffer = {boost::asio::buffer(header.get(), sizeof(ClientDataHeader)), boost::asio::buffer(data.get(), sizeof(TransmittedData))};

            // Perform asynchronous read call
            std::shared_ptr<boost::asio::ip::udp::endpoint> client_endpoint(new boost::asio::ip::udp::endpoint());
            socket_.async_receive_from(buffer, *client_endpoint, 
                boost::bind(&LaserTagServer::onReceive, this, _1, _2, client_endpoint, header, data));
        }

        void onReceive(const boost::system::error_code &error, size_t bytes_transferred, std::shared_ptr<boost::asio::ip::udp::endpoint> client_endpoint,
                std::shared_ptr<ClientDataHeader> header, std::shared_ptr<TransmittedData> data) { 
            // Process client data from async receive
            if (!error) {
                if (header->request) {
                    NewSession(*client_endpoint);
                } else {
                    // Fetch client
                    LaserTagClientSession &update_session = client_sessions_.find(data->player_num)->second; // TODO if session doesn't exist will crash
                    
                    // If we successfully update their data (i.e. data is valid and recent) and they are shooting, check for collisions
                    update_session.UpdateClientState(header->seq_num, *data); 
                    
                    // If client is firing laser, do that
                    if(update_session.GetPlayer().Laser()) {
                        Laser(update_session);
                    }
                }
            }

            // Receive next client data
            Receive();
        }

        void NewSession(boost::asio::ip::udp::endpoint &endpoint) {
            // Add new client to game
            Team team = red_team_count_ > blue_team_count_ ? blue : red;
            TransmittedData new_data;
            new_data.player_num = player_count_;
            new_data.team = team;
            new_data.laser = false;
            LaserTagClientSession new_session(endpoint, new_data);
            client_sessions_.insert(std::pair<int, LaserTagClientSession>(player_count_, new_session));
            
            std::cout << "Added client session " << player_count_ << " at " << new_session.GetEndpoint().address() << std::endl;
            
            // Update counters
            player_count_++;
            if (team == blue) { 
                blue_team_count_++;
            } else { 
                red_team_count_++;
            }
        }

        void Laser(LaserTagClientSession &firing_session) {
            // Get firing player
            const Player &firing = firing_session.GetPlayer();

            // Iterate through the opponents
            for (auto iter = client_sessions_.begin(); iter != client_sessions_.end(); iter++) {
                // Extract the session and player from key value pair
                LaserTagClientSession &opponent_session = iter->second;
                const Player &opponent = opponent_session.GetPlayer();

                // If this player is an opponent
                if (opponent.Team() != firing.Team()) {
                    // If the laser intersects with the opponent
                    if (VectorIntersectsConvexPolygon(opponent.Vertices(), firing.Position(), firing.Direction())) {
                        // Spawn the opponent
                        opponent_session.Spawn();

                        // Update the score
                        if (firing.Team() == blue) 
                            blue_score_++; 
                        else 
                            red_score_++;
                    }
                }
            }
        }

        void Send(const boost::system::error_code &error) {
            // Get state of game
            std::shared_ptr<std::vector<TransmittedData>> game_state = GameState();
            
            // Send state of game to all clients
            for (auto iter = client_sessions_.begin(); iter != client_sessions_.end(); iter++) {
                // Create header for specific client
                std::shared_ptr<ServerDataHeader> header(new ServerDataHeader());
                header->client_player_num = iter->first;
                header->num_players = client_sessions_.size();
                header->red_score = red_score_;
                header->blue_score = blue_score_;
                header->server_seq_num = server_seq_num_;

                // Buffer and write aysnc
                boost::array<boost::asio::const_buffer, 2> buffer = {boost::asio::buffer(header.get(), sizeof(ServerDataHeader)), boost::asio::buffer(*game_state)};
                socket_.async_send_to(buffer, iter->second.GetEndpoint(), boost::bind(&LaserTagServer::OnSend, this, _1, _2, game_state, header));
            }

            // Update server sequence number
            server_seq_num_++;

            // Schedule event to send game state to all clients
            timer_.expires_from_now(boost::posix_time::milliseconds(50));
            timer_.async_wait(boost::bind(&LaserTagServer::Send, this, _1));
        }

        std::shared_ptr<std::vector<TransmittedData>> GameState() {
            // Buffer state of game
            std::shared_ptr<std::vector<TransmittedData>> game_state(new std::vector<TransmittedData>());
            
            // Iterate over the client sessions
            for (auto iter = client_sessions_.begin(); iter != client_sessions_.end(); /* Not while deleting */) {
                if (iter->second.SessionExpired()) {
                    // If client session has expired, remove them from the game
                    std::cout << "Client " << iter->first << " session ended" << std::endl;
                    if (iter->second.ClientState().team == blue) 
                        blue_team_count_--; 
                    else 
                        red_team_count_--;
                    client_sessions_.erase(iter++);
                } else {
                    // Add the client state to the vector
                    game_state->push_back((iter++)->second.ClientState());         
                }
            }

            return game_state;
        }

        void OnSend(const boost::system::error_code &error, size_t bytes_transferred, 
                std::shared_ptr<std::vector<TransmittedData>> game_state, std::shared_ptr<ServerDataHeader> header) {
            // Method maintains ownership of buffer data until async send has completed
        }

    private: 
        // IO
        boost::asio::ip::udp::socket socket_;
        boost::asio::deadline_timer timer_;

        // Hold game data
        std::map<int, LaserTagClientSession> client_sessions_;
        int red_team_count_, blue_team_count_, player_count_;
        int red_score_, blue_score_;
        int server_seq_num_;
};

int main(int argc, char **argv) {
    
    try {
        if (argc < 2) {
            std::cerr << "Usage: TeamBattle <port>" << std::endl;
            return -1;
        } else {
            short port = atoi(argv[1]);
            boost::asio::io_service io_service;
            boost::shared_ptr<LaserTagServer> server(new LaserTagServer(io_service, port));
            std::cout << "Server running" << std::endl;
            io_service.run();
        }
    } catch (std::exception &exc) {
        std::cerr << "Exception: " << exc.what() << std::endl;
    }

    return 0;
}
