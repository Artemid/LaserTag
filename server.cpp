#include <iostream>
#include <map>
#include <vector>
#include <cmath>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/random.hpp>
#include "protocol.hpp"

using namespace CommProtocol;

class TeamBattleClientSession {
    public:
        TeamBattleClientSession(boost::asio::ip::udp::endpoint client_endpoint, int player_num, Team team) 
            : endpoint_(client_endpoint), 
              last_received_(boost::posix_time::second_clock::local_time()) {
            // Standard data
            data_.init = false;
            data_.player_num = player_num;
            data_.team = team;
            data_.shooting = false;
            data_.seq_num = -1;

            // Spawn coordinates and direction
            Spawn();
        }

        const TransmittedData GetClientState() {
            return data_;
        }

        bool UpdateClientState(TransmittedData &new_data) {
            // Update time
            last_received_ = boost::posix_time::second_clock::local_time();
            
            // Check sequence number
            if (new_data.seq_num < data_.seq_num) {
                return false;
            }

            // Check euclidean distance between client and server is tolerable
            float euclidean_distance = sqrtf((data_.x_pos - new_data.x_pos) * (data_.x_pos - new_data.x_pos) + (data_.y_pos - new_data.y_pos) * (data_.y_pos - new_data.y_pos)); 
            if (euclidean_distance < 5) {
                data_ = new_data;
                return true;
            }

            return false;
        }

        const boost::asio::ip::udp::endpoint &GetEndpoint() {
            return endpoint_;
        }

        bool SessionExpired() {
            boost::posix_time::time_duration duration = boost::posix_time::second_clock::local_time() - last_received_;
            return duration.seconds() > 2;
        }

        void Spawn() {
            // Random number generator
            boost::posix_time::ptime time = boost::posix_time::microsec_clock::local_time();
            boost::posix_time::time_duration duration(time.time_of_day());
            boost::mt19937 rng(duration.total_milliseconds());

            // Random coordinates in game 
            boost::uniform_real<> coord_distr(-250, 250);
            boost::variate_generator<boost::mt19937 &, boost::uniform_real<>> coord_random(rng, coord_distr);
            data_.x_pos = coord_random();
            data_.y_pos = coord_random();

            // Random direction
            boost::uniform_int<> dir_distr(0, 71); // 5 degree turns (72 between 0 and 360)
            boost::variate_generator<boost::mt19937 &, boost::uniform_int<>> dir_random(rng, dir_distr);
            float theta = dir_random() * 5 * 4.0 * atan(1.0) / 180.0;
            data_.dir_x = cos(theta);
            data_.dir_y = sin(theta); 
        }

    private:
        boost::asio::ip::udp::endpoint endpoint_;
        boost::posix_time::ptime last_received_;
        
        TransmittedData data_;
};

class TeamBattleServer {
    public:
        TeamBattleServer(boost::asio::io_service &io_service, short port) 
                : socket_(io_service, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), port)), 
                  timer_(io_service) {
            // Initialize variables
            player_count_ = red_team_count_ = blue_team_count_ = red_score_ = blue_score_ = 0;
            server_seq_num_ = 0;

            // Begin sending game state to clients
            timer_.expires_from_now(boost::posix_time::milliseconds(50));
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
                    Team team = red_team_count_ > blue_team_count_ ? blue : red;
                    TeamBattleClientSession new_session(*client_endpoint, player_count_, team);
                    client_sessions_.insert(std::pair<int, TeamBattleClientSession>(player_count_, new_session));
                    std::cout << "Added client session " << player_count_ << " at " << new_session.GetEndpoint().address() << std::endl;
                    
                    // Update counters
                    if (team) blue_team_count_++; else red_team_count_++;
                    player_count_++;
                
                } else {
                    // Fetch client
                    TeamBattleClientSession &update_session = client_sessions_.find(client_data->player_num)->second; // TODO if session doesn't exist will crash
                    
                    // If we successfully update their data (i.e. data is valid and recent) and they are shooting, check for collisions
                    bool updated_successfully = update_session.UpdateClientState(*client_data); 
                    if(updated_successfully && client_data->shooting) {
                        Shoot(*client_data);
                    }
                }
            }

            // Receive next client data
            Receive();
        }

        void Shoot(TransmittedData &shooter) {
            for (auto iter = client_sessions_.begin(); iter != client_sessions_.end(); iter++) {
                if (iter->second.GetClientState().team != shooter.team) {
                    // This player is an opponent
                    TeamBattleClientSession &opponent_session = iter->second;
                    const TransmittedData &player = opponent_session.GetClientState();

                    // Get the vertices of the player's triangle (anticlockwise)
                    std::vector<std::pair<float, float>> triangle_points;
                    triangle_points.push_back(std::pair<float, float>(player.x_pos + player.dir_x * 10, player.y_pos + player.dir_y * 10)); // Nose
                    triangle_points.push_back(std::pair<float, float>(player.x_pos - player.dir_y * 5, player.y_pos + player.dir_x * 5)); // Left wing
                    triangle_points.push_back(std::pair<float, float>(player.x_pos + player.dir_y * 5, player.y_pos - player.dir_x * 5)); // Right wing
                    
                    // Algorithm for determining if ray lies within triangle
                    std::pair<float, float> point(shooter.x_pos, shooter.y_pos);
                    std::pair<float, float> direction(shooter.dir_x, shooter.dir_y);
                    if (RayIntersectsConvexPolygon(triangle_points, point, direction)) {
                        opponent_session.Spawn();
                        if (shooter.team == blue) 
                            blue_score_++; 
                        else 
                            red_score_++;
                    }
                }
            }
        }

        void Send(const boost::system::error_code &error) {
            // Buffer state of game
            std::shared_ptr<std::vector<TransmittedData>> game_state(new std::vector<TransmittedData>());
            for (auto iter = client_sessions_.begin(); iter != client_sessions_.end(); /* Not while deleting */) {
                if (iter->second.SessionExpired()) {
                    std::cout << "Client " << iter->first << " session expired" << std::endl;
                    if (iter->second.GetClientState().team == blue) 
                        blue_team_count_--; 
                    else 
                        red_team_count_--;
                    client_sessions_.erase(iter++);
                } else {
                    game_state->push_back((iter++)->second.GetClientState());         
                }
            }

            // Send state of game to all clients
            for (auto iter = client_sessions_.begin(); iter != client_sessions_.end(); iter++) {
                // Create header for specific client
                std::shared_ptr<TransmittedDataHeader> header(new TransmittedDataHeader());
                header->client_player_num = iter->first;
                header->num_players = client_sessions_.size();
                header->red_score = red_score_;
                header->blue_score = blue_score_;
                header->server_seq_num = server_seq_num_;

                // Buffer and write aysnc
                boost::array<boost::asio::const_buffer, 2> buffer = {boost::asio::buffer(header.get(), sizeof(TransmittedDataHeader)), boost::asio::buffer(*game_state)};
                socket_.async_send_to(buffer, iter->second.GetEndpoint(), boost::bind(&TeamBattleServer::OnSend, this, _1, _2, game_state, header));
            }

            // Update server sequence number
            server_seq_num_++;

            // Schedule event to send game state to all clients
            timer_.expires_from_now(boost::posix_time::milliseconds(50));
            timer_.async_wait(boost::bind(&TeamBattleServer::Send, this, _1));
        }

        void OnSend(const boost::system::error_code &error, size_t bytes_transferred, 
                std::shared_ptr<std::vector<TransmittedData>> game_state, std::shared_ptr<TransmittedDataHeader> header) {
            // Method maintains ownership of buffer data until async send has completed
        }

    private:
        // Helper function returns whether ray intersects convex polygon
        bool RayIntersectsConvexPolygon(const std::vector<std::pair<float, float>> &vertices, const std::pair<float, float> &point, const std::pair<float, float> &direction) {
            float tnear = 0.0; // Prevents intersection detection behind ray
            float tfar = 1000.0; // Limit of ray's reach (set to large value)
            for (int i = 0, j = vertices.size() - 1; i < vertices.size(); j = i, i++) {
                std::pair<float, float> const &e0 = vertices[j]; // vertex 0
                std::pair<float, float> const &e1 = vertices[i]; // vertex 1
                std::pair<float, float> e(e1.first - e0.first, e1.second - e0.second); // v = v1 - v0
                std::pair<float, float> en(e.second, -e.first); // normal to v
                std::pair<float, float> d(e0.first - point.first, e0.second - point.second); // d = v0 - p
                float numer = d.first * en.first + d.second * en.second;
                float denom = direction.first * en.first + direction.second * en.second;

                float tclip = numer / denom; // t = ((v0 - p) . normal) / (dir . normal) 
                if (denom < 0.0f) {
                    if (tclip > tfar)
                        return false;
                    if (tclip > tnear)
                        tnear = tclip;
                } else {
                    if (tclip < tnear)
                        return false;
                    if (tclip < tfar)
                        tfar = tclip;
                }
            }

            return true;
        }
        
        // IO
        boost::asio::ip::udp::socket socket_;
        boost::asio::deadline_timer timer_;

        // Hold game data
        std::map<int, TeamBattleClientSession> client_sessions_;
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
            boost::shared_ptr<TeamBattleServer> server(new TeamBattleServer(io_service, port));
            std::cout << "Server running" << std::endl;
            io_service.run();
        }
    } catch (std::exception &exc) {
        std::cerr << "Exception: " << exc.what() << std::endl;
    }

    return 0;
}
