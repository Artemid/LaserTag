#include <iostream>
#include <string>
#include <cmath>
#include <thread>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <OpenGL/OpenGL.h>
#include <GLUT/GLUT.h>

#include "protocol.hpp"
#include "geometry.cpp"

using namespace CommProtocol;
using namespace Geometry;

typedef enum {
    Up = 101,
    Left = 100,
    Right = 102,
    Down = 103,
    Shoot = 32
} Input;

class TeamBattleClientSession {
    public:
        TeamBattleClientSession(boost::asio::io_service &io_service, boost::asio::ip::udp::endpoint endpoint) 
            : socket_(io_service, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0)), 
              endpoint_(endpoint),
              timeout_timer_(io_service),
              send_timer_(io_service),
              shoot_timer_(io_service),
              other_player_data_(new std::vector<TransmittedData>()) {
            // Send initial packet to server
            RequestEnterGame();
        }

        std::vector<TransmittedData> GetGameState() {
            return *other_player_data_;
        }

        std::pair<int, int> GetScore() {
            return std::pair<int, int>(red_score_, blue_score_);
        }

        void UpdateState(Input input) {
            // Lock data
            mutex_.lock();

            // Get position and direction
            Vector2D pos(my_data_.x_pos, my_data_.y_pos);
            Vector2D dir(my_data_.dir_x, my_data_.dir_y);

            // Handle input
            switch (input) {
                case (Up) : {
                    Vector2D new_pos = pos + dir;
                    my_data_.x_pos = new_pos.x;
                    my_data_.y_pos = new_pos.y;
                    break;
                }
                case (Down) : {
                    Vector2D new_pos = pos - dir;
                    my_data_.x_pos = new_pos.x;
                    my_data_.y_pos = new_pos.y;
                    break;    
                }
                case (Left) : {
                    Vector2D new_dir = RotateDegrees(dir, 5);
                    my_data_.dir_x = new_dir.x;
                    my_data_.dir_y = new_dir.y;
                    break;
                }
                case (Right) : {
                    Vector2D new_dir = RotateDegrees(dir, -5);
                    my_data_.dir_x = new_dir.x;
                    my_data_.dir_y = new_dir.y;
                    break;
                }
                case (Shoot) : {
                    OnShoot();
                }
                default:
                    break;
            }

            // Unlock
            mutex_.unlock();
        }

    private:
        void RequestEnterGame() {
            // Create and send request packet to server
            TransmittedData request_enter_game;
            request_enter_game.init = true;
            std::shared_ptr<TransmittedData> request(new TransmittedData(request_enter_game));
            socket_.async_send_to(boost::asio::buffer(request.get(), sizeof(TransmittedData)), endpoint_, 
                    boost::bind(&TeamBattleClientSession::OnRequestEnterGame, this, _1, _2, request));
        }

        void OnRequestEnterGame(const boost::system::error_code &error, size_t bytes_transferred, std::shared_ptr<TransmittedData> request) {
            // Begin receiving game data with initial flag set
            ReceiveGameData(true);

            // Set a timer to re-request entry to the game if we time out
            timeout_timer_.expires_from_now(boost::posix_time::seconds(1));
            timeout_timer_.async_wait(boost::bind(&TeamBattleClientSession::OnEnterGameTimeout, this, _1));
        }

        void OnEnterGameTimeout(const boost::system::error_code &error) {
            if (error) {
                // Timer was cancelled i.e. game data was received
                return;
            }

            // Try to enter the game again
            RequestEnterGame();
        }

        void ReceiveGameData(bool initial) {
            // Receive game data in form of header data and vector of player states
            std::shared_ptr<TransmittedDataHeader> header(new TransmittedDataHeader());
            std::shared_ptr<std::vector<TransmittedData>> data(new std::vector<TransmittedData>(32));
            boost::array<boost::asio::mutable_buffer, 2> buffer = {boost::asio::buffer(header.get(), sizeof(TransmittedDataHeader)), boost::asio::buffer(*data)};
            
            // Special work to do if this is the initial receiving of data
            if (initial) {
                socket_.async_receive_from(buffer, endpoint_, boost::bind(&TeamBattleClientSession::OnReceiveInitialGameData, this, _1, _2, header, data));
            } else {
                socket_.async_receive_from(buffer, endpoint_, boost::bind(&TeamBattleClientSession::OnReceiveGameData, this, _1, _2, header, data));
            }
        }

        void OnReceiveInitialGameData(const boost::system::error_code &error, size_t bytes_transmitted,
                std::shared_ptr<TransmittedDataHeader> transmitted_data_header, std::shared_ptr<std::vector<TransmittedData>> transmitted_data) {
            // Cancel timer after we receive game data
            timeout_timer_.cancel();

            // Get the sequence number of server
            last_server_seq_num_ = transmitted_data_header->server_seq_num;

            // Get our data
            int my_num = transmitted_data_header->client_player_num;
            my_data_ = *std::find_if(transmitted_data->begin(), transmitted_data->end(), [my_num](const TransmittedData &data) -> bool {
                return data.player_num == my_num;
            });
        
            // Begin sending current data
            send_timer_.expires_from_now(boost::posix_time::milliseconds(50));
            send_timer_.async_wait(boost::bind(&TeamBattleClientSession::SendPlayerData, this, _1));
            
            // Receive data as usual
            OnReceiveGameData(error, bytes_transmitted, transmitted_data_header, transmitted_data);
        }

        void OnReceiveGameData(const boost::system::error_code &error, size_t bytes_transmitted, 
                std::shared_ptr<TransmittedDataHeader> transmitted_data_header, std::shared_ptr<std::vector<TransmittedData>> transmitted_data) {
            // Check sequence number of header is in the correct order
            if (transmitted_data_header->server_seq_num > last_server_seq_num_) {
            
                // Update sequence number
                last_server_seq_num_ = transmitted_data_header->server_seq_num;

                // Fetch data from buffer
                transmitted_data->resize(transmitted_data_header->num_players);

                // If server has corrected our position, update locally
                int my_num = transmitted_data_header->client_player_num;
                TransmittedData new_my_data = *std::find_if(transmitted_data->begin(), transmitted_data->end(), [my_num](const TransmittedData &data) -> bool {
                        return data.player_num == my_num;
                });

                float norm = Norm(Vector2D(my_data_.x_pos, my_data_.y_pos) - Vector2D(new_my_data.x_pos, new_my_data.y_pos));
                if (norm > 5) {
                    my_data_.x_pos = new_my_data.x_pos;
                    my_data_.y_pos = new_my_data.y_pos;
                    my_data_.dir_x = new_my_data.dir_x;
                    my_data_.dir_y = new_my_data.dir_y;
                }

                // Update member variables
                other_player_data_ = transmitted_data;
                red_score_ = transmitted_data_header->red_score;
                blue_score_ = transmitted_data_header->blue_score;
            }

            // Receive next
            ReceiveGameData(false);
        }

        void SendPlayerData(const boost::system::error_code &error) {
            // Create packet
            std::shared_ptr<TransmittedData> curr_data(new TransmittedData(my_data_));
            
            // Send asynchronously
            socket_.async_send_to(boost::asio::buffer(curr_data.get(), sizeof(TransmittedData)), endpoint_, 
                    boost::bind(&TeamBattleClientSession::OnSendPlayerData, this, _1, _2, curr_data));
        }

        void OnSendPlayerData(const boost::system::error_code &error, size_t bytes_transmitted, std::shared_ptr<TransmittedData> data) {
            // Update sequence number if send was successful
            if (!error) {
                my_data_.seq_num++;
            }
            
            // Timer for the next send
            send_timer_.expires_from_now(boost::posix_time::milliseconds(50));
            send_timer_.async_wait(boost::bind(&TeamBattleClientSession::SendPlayerData, this, _1));
        }

        // Helper function for shooting
        void OnShoot() {
            my_data_.shooting = true;
            shoot_timer_.expires_from_now(boost::posix_time::milliseconds(250));
            shoot_timer_.async_wait([this](const boost::system::error_code &error) {
                if (!error) {
                    this->my_data_.shooting = false;
                }
            });
        }

        // IO member variables
        boost::asio::ip::udp::socket socket_;
        boost::asio::ip::udp::endpoint endpoint_;
        boost::asio::deadline_timer timeout_timer_;
        boost::asio::deadline_timer send_timer_;
        boost::asio::deadline_timer shoot_timer_;

        // Lock for OpenGL accesses
        std::mutex mutex_;

        // Hold my data
        TransmittedData my_data_;
        int red_score_, blue_score_;
        std::shared_ptr<std::vector<TransmittedData>> other_player_data_;
        int last_server_seq_num_;
};

/*
 * OpenGL
 */

TeamBattleClientSession *global_session_ptr;

void Render() {
    // Clear color
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   
    // Draw triangle for each player
    std::vector<TransmittedData> players = global_session_ptr->GetGameState();
    for (TransmittedData player : players) {
        glBegin(GL_TRIANGLES);
        // Color of the player
        if (player.team == blue)
            // Blue
            glColor3f(0.0, 1.0, 1.0);
        else
            // Red
            glColor3f(1.0, 0.0, 0.0);
        
        // Nose of player
        float point_x = (player.x_pos + player.dir_x * 10);
        float point_y = (player.y_pos + player.dir_y * 10);
        glVertex2f(point_x, point_y);

        // Right wing of player
        float left_x = (player.x_pos + player.dir_y * 5);
        float left_y = (player.y_pos - player.dir_x * 5);
        glVertex2f(left_x, left_y);
        
        // Left wing of player
        float right_x = (player.x_pos - player.dir_y * 5);
        float right_y = (player.y_pos + player.dir_x * 5);
        glVertex2f(right_x, right_y);

        glEnd();

        // Shooting
        if (player.shooting) {
            glBegin(GL_LINES);
              glVertex2f(player.x_pos, player.y_pos);
              glVertex2f(player.x_pos + player.dir_x * 1000, player.y_pos + player.dir_y * 1000);
            glEnd();
        }
    }

    // Draw score
    std::pair<int, int> score = global_session_ptr->GetScore();
    std::stringstream ss;
    ss << "Red " << score.first << " — " << score.second << " Blue";
    std::string tmp = ss.str();
    const char *cstr = tmp.c_str();
    glutSetWindowTitle(cstr);

    // Swap buffers to submit
    glutSwapBuffers();
}

void KeyboardInput(int key, int x, int y) {
    global_session_ptr->UpdateState(static_cast<Input>(key));
}

/*
 * MAIN
 */

int main(int argc, char **argv) {
    try {
        if (argc < 3) {
            std::cerr << "Usage: TeamBattleClient <remote_address> <remote_port>" << std::endl;
            return -1;
        } else {
            // Get server endpoint TODO put this in session class
            boost::asio::io_service io_service;
            boost::asio::ip::udp::resolver resolver(io_service);
            boost::asio::ip::udp::resolver::query query(boost::asio::ip::udp::v4(), argv[1], argv[2]);
            boost::asio::ip::udp::endpoint endpoint = *resolver.resolve(query);

            // Run network io on separate thread
            TeamBattleClientSession client(io_service, endpoint);
            global_session_ptr = &client;
            std::thread async_io_thread(boost::bind(&boost::asio::io_service::run, &io_service));
       
            std::cout << "Client running" << std::endl;

            // Initialize openGL
            glutInit(&argc, argv);
            glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);
            glutInitWindowSize(500, 500);
            glutInitWindowPosition(200,200);
            glutCreateWindow("TeamBattle");

            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            gluOrtho2D(-250, 250, -250, 250);

            glutDisplayFunc(Render);
            glutIdleFunc(Render);
            glutSpecialFunc(KeyboardInput);

            glutMainLoop();
        }
    } catch (std::exception exc) {
        std::cerr << "Exception: " << exc.what() << std::endl;
    }
}
