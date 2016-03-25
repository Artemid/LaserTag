#include <iostream>
#include <cmath>
#include <thread>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <OpenGL/OpenGL.h>
#include <GLUT/GLUT.h>

struct TransmittedDataHeader {
    int client_player_num;
    int num_players;
};

struct TransmittedData {
    int init; // True if player is entering game
    int player_num;
    float x_pos;
    float y_pos;
    float dir_x;
    float dir_y;
    int seq_num;
};

typedef enum {
    Up = 101,
    Left = 100,
    Right = 102,
    Down = 103
} Input;

class TeamBattleClientSession {
    public:
        TeamBattleClientSession(boost::asio::io_service &io_service, boost::asio::ip::udp::endpoint endpoint) 
            : socket_(io_service, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0)), 
              endpoint_(endpoint),
              timeout_timer_(io_service),
              send_timer_(io_service) {
            // Send initial packet to server
            RequestEnterGame();
        }

        std::vector<TransmittedData> GetGameState() {
            return *other_player_data_;
        }

        void UpdateState(Input input) {
            // Lock data
            mutex_.lock();

            // Handle input
            switch (input) {
                case (Up) : {
                    my_data_.x_pos += my_data_.dir_x;
                    my_data_.y_pos += my_data_.dir_y;
                    break;
                }
                case (Down) : {
                    my_data_.x_pos -= my_data_.dir_x;
                    my_data_.y_pos -= my_data_.dir_y;
                    break;    
                }
                case (Left) : {
                    std::pair<float, float> new_dir = RotateVector(my_data_.dir_x, my_data_.dir_y, 5);
                    my_data_.dir_x = new_dir.first;
                    my_data_.dir_y = new_dir.second;
                    break;
                }
                case (Right) : {
                    std::pair<float, float> new_dir = RotateVector(my_data_.dir_x, my_data_.dir_y, -5);
                    my_data_.dir_x = new_dir.first;
                    my_data_.dir_y = new_dir.second;
                    break;
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
            socket_.async_send_to(boost::asio::buffer(request.get(), sizeof(TransmittedData)), endpoint_, boost::bind(&TeamBattleClientSession::OnRequestEnterGame, this, _1, _2, request));
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

            // Get our data
            int my_num = transmitted_data_header->client_player_num;
            my_data_ = *std::find_if(transmitted_data->begin(), transmitted_data->end(), [my_num](const TransmittedData &data) -> bool {return data.player_num == my_num;});
        
            // Begin sending current data
            send_timer_.expires_from_now(boost::posix_time::milliseconds(50));
            send_timer_.async_wait(boost::bind(&TeamBattleClientSession::SendPlayerData, this, _1));
            
            // Receive data as usual
            OnReceiveGameData(error, bytes_transmitted, transmitted_data_header, transmitted_data);
        }

        void OnReceiveGameData(const boost::system::error_code &error, size_t bytes_transmitted, 
                std::shared_ptr<TransmittedDataHeader> transmitted_data_header, std::shared_ptr<std::vector<TransmittedData>> transmitted_data) {
            // Fetch data from buffer
            transmitted_data->resize(transmitted_data_header->num_players);
     
            // Update member variables
            other_player_data_ = transmitted_data;

            // Receive next
            ReceiveGameData(false);
        }

        void SendPlayerData(const boost::system::error_code &error) {
            // Create packet
            std::shared_ptr<TransmittedData> curr_data(new TransmittedData(my_data_));
            
            // Send asynchronously
            socket_.async_send_to(boost::asio::buffer(curr_data.get(), sizeof(TransmittedData)), endpoint_, boost::bind(&TeamBattleClientSession::OnSendPlayerData, this, _1, _2, curr_data));
        }

        void OnSendPlayerData(const boost::system::error_code &error, size_t bytes_transmitted, std::shared_ptr<TransmittedData> data) {
            // Timer for the next send
            send_timer_.expires_from_now(boost::posix_time::milliseconds(50));
            send_timer_.async_wait(boost::bind(&TeamBattleClientSession::SendPlayerData, this, _1));
        }

        // Helper function to rotate direction vector
        std::pair<float, float> RotateVector(float x, float y, float degrees) {
            float theta = degrees * 4.0 * atan(1.0) / 180.0;
            float cos = cosf(theta);
            float sin = sinf(theta);
            float x_new = x * cos - y * sin;
            float y_new = x * sin + y * cos;
            return std::pair<float, float>(x_new, y_new);
        }

        // IO member variables
        boost::asio::ip::udp::socket socket_;
        boost::asio::ip::udp::endpoint endpoint_;
        boost::asio::deadline_timer timeout_timer_;
        boost::asio::deadline_timer send_timer_;

        // Lock for OpenGL accesses
        std::mutex mutex_;

        // Hold my data
        TransmittedData my_data_;
        std::shared_ptr<std::vector<TransmittedData>> other_player_data_;
};

/*
 * OpenGL
 */

TeamBattleClientSession *global_session_ptr;

void Render() {
    // Clear color
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Draw triangle for each player
    glBegin(GL_TRIANGLES);
    std::vector<TransmittedData> players = global_session_ptr->GetGameState();
    for (TransmittedData player : players) {
        // Color of the player
        glColor3f(1.0, 1.0, 1.0);
        
        // Point of player
        float point_x = (player.x_pos + player.dir_x) * 0.01;
        float point_y = (player.y_pos + player.dir_y) * 0.01;
        glVertex2f(point_x, point_y);

        // Right wing of player
        float left_x = (player.x_pos + player.dir_y / 2.0) * 0.01;
        float left_y = (player.y_pos - player.dir_x / 2.0) * 0.01;
        glVertex2f(left_x, left_y);
        
        // Left wing of player
        float right_x = (player.x_pos - player.dir_y / 2.0) * 0.01;
        float right_y = (player.y_pos + player.dir_x / 2.0) * 0.01;
        glVertex2f(right_x, right_y);
    }
    glEnd();

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
        
            // Initialize openGL
            glutInit(&argc, argv);
            glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);
            glutInitWindowSize(1000, 1000);
            glutInitWindowPosition(200,200);
            glutCreateWindow("TeamBattle");

            glutDisplayFunc(Render);
            glutIdleFunc(Render);
            glutSpecialFunc(KeyboardInput);

            glutMainLoop();
        }
    } catch (std::exception exc) {
        std::cerr << "Exception: " << exc.what() << std::endl;
    }
}
