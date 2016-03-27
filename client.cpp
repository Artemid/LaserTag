#include <iostream>
#include <string>
#include <set>
#include <cmath>
#include <thread>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <OpenGL/OpenGL.h>
#include <GLUT/GLUT.h>

#include "player.cpp"

using namespace CommProtocol;
using namespace Geometry;

typedef enum {
    Up = 101,
    Left = 100,
    Right = 102,
    Down = 103,
    Space = 32
} Input;

class LaserTagClient {
    public:
        LaserTagClient(boost::asio::io_service &io_service, boost::asio::ip::udp::endpoint endpoint) 
            : socket_(io_service, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), 0)), 
              endpoint_(endpoint),
              timeout_timer_(io_service),
              send_timer_(io_service),
              laser_timer_(io_service),
              players_(std::map<int, Player>()), 
              seq_num_(0) {
            // Send initial packet to server
            RequestEnterGame();
        }

        std::map<int, Player> &Players() {
            return players_;
        }

        std::pair<int, int> GetScore() {
            return std::pair<int, int>(red_score_, blue_score_);
        }

        int GetPlayerNum() {
            return my_player_num_;
        }

        void UpdateState(Input input) {
            // Lock data
            mutex_.lock();

            // Handle input
            switch (input) {
                case (Up) : {
                    MyPlayer().MoveForward();
                    break;
                }
                case (Down) : {
                    MyPlayer().MoveBackward();
                    break;    
                }
                case (Left) : {
                    MyPlayer().RotateLeft();
                    break;
                }
                case (Right) : {
                    MyPlayer().RotateRight();
                    break;
                }
                case (Space) : {
                    Laser();
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
            std::shared_ptr<ClientDataHeader> request(new ClientDataHeader());
            request->request = true;
            socket_.async_send_to(boost::asio::buffer(request.get(), sizeof(ClientDataHeader)), endpoint_, 
                    boost::bind(&LaserTagClient::OnRequestEnterGame, this, _1, _2, request));
        }

        void OnRequestEnterGame(const boost::system::error_code &error, size_t bytes_transferred, std::shared_ptr<ClientDataHeader> request) {
            // Begin receiving game data with initial flag set
            ReceiveGameData(true);

            // Set a timer to re-request entry to the game if we time out
            timeout_timer_.expires_from_now(boost::posix_time::seconds(1));
            timeout_timer_.async_wait(boost::bind(&LaserTagClient::OnEnterGameTimeout, this, _1));
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
            std::shared_ptr<ServerDataHeader> header(new ServerDataHeader());
            std::shared_ptr<std::vector<TransmittedData>> data(new std::vector<TransmittedData>(32));
            boost::array<boost::asio::mutable_buffer, 2> buffer = {boost::asio::buffer(header.get(), sizeof(ServerDataHeader)), boost::asio::buffer(*data)};
            
            // Special work to do if this is the initial receiving of data
            if (initial) {
                socket_.async_receive_from(buffer, endpoint_, boost::bind(&LaserTagClient::OnReceiveInitialGameData, this, _1, _2, header, data));
            } else {
                socket_.async_receive_from(buffer, endpoint_, boost::bind(&LaserTagClient::OnReceiveGameData, this, _1, _2, header, data));
            }
        }

        void OnReceiveInitialGameData(const boost::system::error_code &error, size_t bytes_transmitted,
                std::shared_ptr<ServerDataHeader> transmitted_data_header, std::shared_ptr<std::vector<TransmittedData>> transmitted_data) {
            // Cancel timer after we receive game data
            timeout_timer_.cancel();

            // Get the sequence number of server
            last_server_seq_num_ = transmitted_data_header->server_seq_num;

            // Get our data
            my_player_num_ = transmitted_data_header->client_player_num;
            
            // Receive data as usual
            OnReceiveGameData(error, bytes_transmitted, transmitted_data_header, transmitted_data);
            
            // Begin sending current data
            send_timer_.expires_from_now(boost::posix_time::milliseconds(50));
            send_timer_.async_wait(boost::bind(&LaserTagClient::SendPlayerData, this, _1));
        }

        void OnReceiveGameData(const boost::system::error_code &error, size_t bytes_transmitted, 
                std::shared_ptr<ServerDataHeader> transmitted_data_header, std::shared_ptr<std::vector<TransmittedData>> transmitted_data) {
            // Check sequence number of header is in the correct order
            if (transmitted_data_header->server_seq_num > last_server_seq_num_) {
            
                // Update header variables
                last_server_seq_num_ = transmitted_data_header->server_seq_num;
                red_score_ = transmitted_data_header->red_score;
                blue_score_ = transmitted_data_header->blue_score;

                // Fetch data from buffer into our map
                transmitted_data->resize(transmitted_data_header->num_players);
                std::set<int> active_players;
                for (TransmittedData player_data : *transmitted_data) {
                    InsertOrUpdatePlayer(player_data.player_num, player_data);
                    active_players.insert(player_data.player_num);
                }

                // Remove inactive players
                for (auto iter = players_.begin(); iter != players_.end(); /* */) {
                    if (active_players.find(iter->first) == active_players.end()) {
                        players_.erase(iter++);
                    } else {
                        iter++;
                    }
                }
            }

            // Receive next
            ReceiveGameData(false);
        }

        void InsertOrUpdatePlayer(int player_num, TransmittedData &data) {
            auto iter = players_.find(player_num);
            if (iter == players_.end()) {
                players_.insert(std::make_pair(player_num, Player(data)));
            } else {
                if (player_num == my_player_num_) {
                    // If we are updating ourselves, only change to server coordinates if we moved a long distance (i.e. respawned)
                    Vector2D new_pos(data.x_pos, data.y_pos);
                    Vector2D cur_pos = iter->second.Position();
                    if (Norm(new_pos - cur_pos) > 5) {
                        iter->second.Update(data);
                    }
                } else {
                    iter->second.Update(data);
                }
            }
        }

        void SendPlayerData(const boost::system::error_code &error) {
            // Create packet
            std::shared_ptr<ClientDataHeader> header(new ClientDataHeader());
            header->request = false;
            header->seq_num = seq_num_++;
            std::shared_ptr<TransmittedData> data(new TransmittedData(MyPlayer().Data()));
            boost::array<boost::asio::const_buffer, 2> buffer = {boost::asio::buffer(header.get(), sizeof(ClientDataHeader)), boost::asio::buffer(data.get(), sizeof(TransmittedData))};

            // Send asynchronously
            socket_.async_send_to(buffer, endpoint_, boost::bind(&LaserTagClient::OnSendPlayerData, this, _1, _2, header,data));
        }

        void OnSendPlayerData(const boost::system::error_code &error, size_t bytes_transmitted, std::shared_ptr<ClientDataHeader> header, std::shared_ptr<TransmittedData> data) {
            // Timer for the next send
            send_timer_.expires_from_now(boost::posix_time::milliseconds(50));
            send_timer_.async_wait(boost::bind(&LaserTagClient::SendPlayerData, this, _1));
        }

        // Helper function for laser
        void Laser() {
            MyPlayer().SetLaser(true);
            laser_timer_.expires_from_now(boost::posix_time::milliseconds(250));
            laser_timer_.async_wait([this](const boost::system::error_code &error) {
                if (!error) {
                    this->MyPlayer().SetLaser(false);
                }
            });
        }

        Player &MyPlayer() {
            return players_.find(my_player_num_)->second;
        }

        // IO member variables
        boost::asio::ip::udp::socket socket_;
        boost::asio::ip::udp::endpoint endpoint_;
        boost::asio::deadline_timer timeout_timer_;
        boost::asio::deadline_timer send_timer_;
        boost::asio::deadline_timer laser_timer_;

        // Lock for OpenGL accesses
        std::mutex mutex_;

        // Hold my data
        int my_player_num_;
        std::map<int, Player> players_;
        int red_score_, blue_score_;
        int last_server_seq_num_;
        int seq_num_;
};

/*
 * OpenGL
 */

LaserTagClient *global_session_ptr;

void Render() {
    // Clear color
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   
    // Get our player number
    int my_num = global_session_ptr->GetPlayerNum();

    // Draw triangle for each player
    std::map<int, Player> players = global_session_ptr->Players();
    for (std::pair<int, Player> k_v : players) {
        // Player vector
        Player &player = k_v.second;

        // Color of the player
        if (player.Team() == blue) {
            if (player.PlayerNum() == my_num)
                glColor3f(0.0, 1.0, 1.0); // Cyan
            else
                glColor3f(0.12, 0.56, 1.0); // Blue
        } else {
            if (player.PlayerNum() == my_num)
                glColor3f(1.0, 0.08, 0.57); // Pink
            else
                glColor3f(1.0, 0.0, 0.0); // Red
        }
        
        // Draw body
        std::vector<Vector2D> vertices = player.Vertices();
        glBegin(GL_TRIANGLES);
        for (Vector2D v : vertices) {
            glVertex2f(v.x, v.y);
        }
        glEnd();

        // Draw line for laser
        if (player.Laser()) {
            const Vector2D &pos = player.Position();
            const Vector2D &dir = player.Direction();
            Vector2D shot_end(pos + dir * 1000);
            glBegin(GL_LINES);
            glVertex2f(pos.x, pos.y);
            glVertex2f(shot_end.x, shot_end.y);
            glEnd();
        }
    }

    // Write score
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
            LaserTagClient client(io_service, endpoint);
            global_session_ptr = &client;
            std::thread async_io_thread(boost::bind(&boost::asio::io_service::run, &io_service));
       
            std::cout << "Client running" << std::endl;

            // Initialize openGL
            glutInit(&argc, argv);
            glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);
            glutInitWindowSize(500, 500);
            glutInitWindowPosition(200,200);
            glutCreateWindow("LaserTag");

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
