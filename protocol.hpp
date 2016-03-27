namespace CommProtocol {

struct TransmittedDataHeader {
    unsigned int client_player_num;
    unsigned int num_players;
    unsigned int red_score;
    unsigned int blue_score;
    unsigned int server_seq_num;
};

typedef enum {
    red = 0,
    blue = 1
} Team;

struct TransmittedData {
    int init; // True if player is entering game
    unsigned int player_num;
    Team team;
    float x_pos;
    float y_pos;
    float dir_x;
    float dir_y;
    int shooting;
    unsigned int seq_num;
};
            
}
