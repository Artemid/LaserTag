namespace CommProtocol {

struct TransmittedDataHeader {
    int client_player_num;
    int num_players;
    int red_score;
    int blue_score;
    int server_seq_num;
};

typedef enum {
    red = 0,
    blue = 1
} Team;

struct TransmittedData {
    int init; // True if player is entering game
    int player_num;
    Team team;
    float x_pos;
    float y_pos;
    float dir_x;
    float dir_y;
    int shooting;
    int seq_num;
};
            
}
