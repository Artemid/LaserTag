#ifndef UI_H
#define UI_H

namespace UI {

    extern std::shared_ptr<LaserTagClient> session_ptr;

    void InitUI();

    void Render();

    void DrawPlayers();

    void WriteScore();

    void KeyboardDown(int key, int x, int y);

    void KeyboardUp(int key, int x, int y);

    void Reshape(int w, int h);
}

#endif
