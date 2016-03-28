#include <iostream>
#include <thread>

#include "client.hpp"
#include "ui.hpp"

int main(int argc, char **argv) {
    try {
        if (argc < 3) {
            std::cerr << "Usage: TeamBattleClient <remote_address> <remote_port>" << std::endl;
            return -1;
        } else {
            // Init io service
            boost::asio::io_service io_service;

            // Run network io on separate thread
            LaserTagClient client(io_service, argv[1], argv[2]);
            
            // Initialize client
            std::thread async_io_thread([&io_service]() {
                io_service.run();
            });
       
            std::cout << "Client running" << std::endl;

            // Initialize UI
            UI::session_ptr = std::shared_ptr<LaserTagClient>(&client);
            UI::InitUI();
        }
    } catch (std::exception exc) {
        std::cerr << "Exception: " << exc.what() << std::endl;
    }
}
