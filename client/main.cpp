#include "client.hpp"

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
