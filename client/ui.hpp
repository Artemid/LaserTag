#ifndef UI_H
#define UI_H

namespace UI {

    extern std::shared_ptr<LaserTagClient> session_ptr;

    void InitUI();

    void Render();

    void DrawPlayers();

    void WriteScore();

    void KeyboardInput();
}

#endif
