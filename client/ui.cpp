#include <algorithm>
#include <OpenGL/OpenGL.h>
#include <GLUT/GLUT.h>

#include "player.hpp"
#include "client.hpp"

#include "ui.hpp"

using namespace Geometry;
using namespace Protocol;

namespace UI {

std::shared_ptr<LaserTagClient> session_ptr;

bool keys[256];

void InitUI() {
    // Initialize openGL
    int argc = 1;
    char *argv[1] = {"LaserTag"};
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
    
    glutIgnoreKeyRepeat(true);
    glutSpecialFunc(KeyboardDown);
    glutSpecialUpFunc(KeyboardUp);

    glutMainLoop();
}

void Render() {
    // Clear color
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   
    // Draw
    DrawPlayers();
    WriteScore();

    // Get controls state
    int controls[5] = {static_cast<int>(Up), static_cast<int>(Down), static_cast<int>(Left), static_cast<int>(Right), static_cast<int>(Space)};
    for (int i = 0; i < 5; i++) {
        if (keys[controls[i]]) {
            session_ptr->UpdateState(static_cast<Input>(controls[i]));
        }
    }

    // Swap buffers to submit
    glutSwapBuffers();
}

void DrawPlayers() {
    // Get our player number
    int my_num = session_ptr->GetPlayerNum();

    // Draw triangle for each player
    std::map<int, Player> players = session_ptr->Players();
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
}

void WriteScore() {
    // Write score
    std::pair<int, int> score = session_ptr->GetScore();
    std::stringstream ss;
    ss << "Red " << score.first << " — " << score.second << " Blue";
    std::string tmp = ss.str();
    const char *cstr = tmp.c_str();
    glutSetWindowTitle(cstr);

}

void KeyboardDown(int key, int x, int y) {
    keys[key] = true;
}

void KeyboardUp(int key, int x, int y) {
    keys[key] = false;
}

void Reshape(int w, int h) {
    float aspect_ratio = float(w) / float(h);
    if (w >= h) {
        gluOrtho2D(-250 * aspect_ratio, 250 * aspect_ratio, -250, 250);
    } else {
        gluOrtho2D(-250, 250, -250 / aspect_ratio, 250 / aspect_ratio);
    }
}

}
