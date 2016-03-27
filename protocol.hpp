namespace CommProtocol {

struct ServerDataHeader {
    unsigned int client_player_num;
    unsigned int num_players;
    unsigned int red_score;
    unsigned int blue_score;
    unsigned int server_seq_num;
};

struct ClientDataHeader {
    int request;
    unsigned int seq_num;
};

typedef enum {
    red = 0,
    blue = 1
} Team;

struct TransmittedData {
    unsigned int player_num;
    Team team;
    float x_pos;
    float y_pos;
    float dir_x;
    float dir_y;
    int laser;
};
            
}
