#include <locale.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h> 
#include <sys/fcntl.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>


#define CURS 0 //visibilità cursore
#define ND 1 //valore di nodelay
#define KP 1 //keypad

//macro dei colori
#define COL_RIVA 1
#define COL_MARCIAPIEDE 2
#define COL_FIUME 3
#define COL_RANA 4
#define COL_RANA_FIUME 10
#define COL_RANA_RIVA 11
#define COL_RANA_MARC 12
#define COL_RANA_EV_MARC 13
#define COL_PROIETTILI_P 5 //colore proiettili piante
#define COL_PR_RANA 6 //colore proiettili rana
#define COL_EV_CROC 7 //colore coccodrilli cattivi
#define COL_TIME 9

//macro testo
#define AVVISO_DIM_SCHERMO "Ingradire lo schermo per iniziare il gioco" //stampato quando la dimensione dello schermo è inferiore al minimo e non si è mezzo alla partita
#define AVVISO DIM_SCHERMO_M "Ingradire lo schermo per continuare il gioco" //stessa cosa ma la partita è iniziata

//macro ID
//IMPORTANTE!!! I macro con il suffisso "F" si riferiscono esclusivamente agli id dei genitori dei coccodrilli e delle piante
//non ai coccodrilli e piante come entità singole
#define FROG_ID 4
#define FCROC_ID 1
#define FPLANT_ID 2
#define CROC_ID 3
#define BULL_ID 5
#define PLANT_ID 6
#define BULL_PL_ID 7
#define EV_CROC_ID 8
#define IMM_CROC_ID 9

//macro dimensioni entità
#define FROG_X 4
#define FROG_Y 3
#define CROC_X 16
#define CROC_Y FROG_Y
#define PLANT_X 3
#define PLANT_Y 3
#define BULL_X 1
#define BULL_Y 2

//macro di gioco
#define TEMPO_MANCHE 60
#define VITE 4
#define N_PIANTE 3
#define N_FLUSSI 8
#define N_TANE 4
#define SYMB_VITE "♥ "
#define TAB 32
#define CROC_DELAY 700000
#define UPPER_CROC_SPEED 35000
#define LOWER_CROC_SPEED 80000
#define BULL_SPEED_DELAY 150000
#define BULLET_SPEED 2
#define PLANT_DELAY 1000000
#define PLANT_STOP_UPPER 5000000
#define PLANT_STOP_LOWER 600000
#define EV_CROC_RATE 2
#define IMMERSION 100

//macro dimensioni grafica (senza contare le entità). I suffisi sx e sy indicano i vertici in alto a sinistra delle varie zone.
#define DIM_X 155 //larghezza campo di gioco
#define DIM_Y 42 //altezza campo di gioco
#define LIM_SUP 6 //limite superiore schermo
#define TIME_START 2 //offset della barra del tempo
#define TIME_Y 1
#define VITEX 1
#define VITEY DIM_Y-2
#define SCOREY VITEY
#define SCOREX DIM_X-6
#define TANEX DIM_X-2
#define TANEY 3
#define RIVA_X DIM_X-2
#define RIVA_Y 4
#define FIUME_X DIM_X
#define FIUME_Y 24
#define MARCIAPIEDE_X DIM_X-2
#define MARCIAPIEDE_Y 4
#define TANESX 1
#define TANESY 3
#define RIVASX 1
#define RIVASY TANESY+TANEY
#define FIUMESX 0
#define FIUMESY RIVASY+RIVA_Y
#define MARCIAPIEDESX 1
#define MARCIAPIEDESY FIUMESY+FIUME_Y
#define FLUX_Y (FIUME_Y/N_FLUSSI)

#define FROG_STARTY (MARCIAPIEDESY)
#define FROG_STARTX ((MARCIAPIEDE_X+CROC_X*2) / 2 - 5)
#define FIRST_PLANT_PY RIVASY+1
#define PLANT_PX RIVASX+7
#define PLANT_DISTANCE 70


struct screen {
   int x;
   int y;
   pid_t pid;
   pid_t pid_frog;
   bool on_croc;
   
};

struct manchestr {
   bool loss;
   int score;
   bool manche;
};

struct msg {
    int id;
    int x;    
    int y;
    pid_t pid; 
    int x_speed;
    int y_speed;  
};

struct entity {
    int id;
    int x;    
    int y;    
    bool first;
    char ch[1];
    pid_t pid; 
    pid_t pid1;
    pid_t pid2;
};

struct match_data {
   int vite;
   int score;
   bool running;
};

struct screen sfrog;

struct manchestr partita;

struct msg entity_data[9];

struct entity game_matrix[DIM_X+2*CROC_X][DIM_Y];

//Questa flag mi serve per avvisare il coccodrillo cattivo che è stato colpito
//stiamo lavorando con i processi quindi, per evitare problemi, voglio che sia atomica
volatile sig_atomic_t evil_flag = 0;

//pid del padre di coccodrilli e piante
pid_t fcroc_pid;
pid_t fplant_pid;



int flussi[N_FLUSSI];

char frog_sprite[FROG_Y*FROG_X][FROG_X] = {"▙", "█", "█" ,"▟", " ", "█", "█", " ", "█","▀", "▀", "█"};
char croc_sprite[CROC_Y*CROC_X][CROC_X] = {" ", "▁", " ", "▗", "◀", " ", "◀", " ", "◀", " ", "◀", " ", "◀", " ", "◀", " ", " ", " ", "◀", " ", "◀", " ", "◀", " ", "◀", " ", "◀", " ", "◀", " ", "◀", " ", " ", "▔", " ", "▝", "◀", " ", "◀", " ", "◀", " ", "◀", " ", "◀", " ", "◀", " "};

char croc_sprite1[CROC_Y*CROC_X][CROC_X] = {" ", "◀", " ", "◀", " ", "◀", " ", "◀", " ", "◀", " ", "◀", "▗", " ", "▁", " ", "◀", " ", "◀", " ", "◀", " ", "◀", " ", "◀", " ", "◀", " ", "◀", " ", " ", " ", " ","◀", " ", "◀", " ", "◀", " ", "◀", " ", "◀", " ", "◀", "▝", " ", "▔", " "};

char plant_sprite[PLANT_Y*PLANT_X][PLANT_X] = {" ","✽"," ", "|", "Ω", "|", "|", "∏", "|"};
char bull_sprite[CROC_Y*CROC_X][CROC_X] = {"▲","•"};

//finestra di gioco
WINDOW *gamewin;
int pipe_fds[2];

//prototipi func
void game_init();
void tane();
void riva();
void fiume();
void marciapiede();
void background();
void display_vite(int);
void display_score(int);
void time_bar(long int);
void miscellaneous_graphics(long int, int, int);
void draw_frog(int, int);
void draw_croc(int, int, int);
void draw_loop();
void generate_graphics(long int, int, int);
long int gettime(long int);
struct screen get_screen_size();
struct screen generate_win_start();
bool is_screen_size_ok();
void screen_size_loop();
bool frog_win();
bool frog_in_water();
void create_game_win();
bool time_is_over(long int);
bool manche_is_over(long int);
void handle_signal_crocs(int);
void handle_signal_plants(int);
void bullet(int, int, int);
void frog();
void croc_creator();
void plant_creator();
void croc(int, bool);
void plant(int);
//void croc_creator(int);
void generate_process(int, int, int);
void generate_frog();
void generate_crocs();
void generate_plants();
void generate_processes();
void send_signal(pid_t pid);
void pause_process(pid_t);
void kill_process(pid_t);
void kill_all();
int which_plant(int);
bool check_bull_collisions(pid_t, int, int, int);
void receive_from_frog();
void receive_from_crocs();
void receive_from_plants();
bool check_collision(int, int, int);
bool is_frog_on_croc();
void update_frog(int, int, pid_t);
void delete_old(struct msg, struct msg);
void update(struct msg, struct msg);
void receive_data();
void init_flux_speed();
void init_pipe();
void ready_frog();
void ready_game_matrix();
void ready_entity_data();
struct manchestr game(bool, long int, int, int);
void run_clean(struct manchestr, struct match_data*);
void run(bool, bool, struct timespec, int, int);
void game_polling();

int main() {
   //funzione che si occupa di inizializzare la schermata e i colori
   game_init();
   //funzione che contiene la logica di gioco
   game_polling();

}

//inizializza schermo e colori
void game_init() {
   //inizializzo lo standard screen. Abbiamo scelto di settare nodelay a false ma non è necessario.
    setlocale(LC_ALL, ""); initscr(); noecho(); curs_set(CURS); nodelay(stdscr, ND); keypad(stdscr, KP); cbreak(); start_color(); 
   //inizializzo colori
   init_pair(COL_RIVA, COLOR_BLUE, COLOR_YELLOW);
   init_pair(COL_MARCIAPIEDE, COLOR_BLACK, COLOR_GREEN);
   init_pair(COL_FIUME, COLOR_YELLOW, COLOR_BLUE);
   init_pair(COL_RANA, COLOR_BLUE, COLOR_CYAN);
   init_pair(COL_PROIETTILI_P, COLOR_BLUE, COLOR_WHITE);
   init_pair(COL_PR_RANA, COLOR_BLUE, COLOR_RED);
   init_pair(COL_EV_CROC, COLOR_RED, COLOR_BLACK);
   init_color(COL_TIME, 10, 10, 80);
   init_pair(COL_RANA_FIUME, COLOR_RED, COLOR_BLUE);
   init_pair(COL_RANA_RIVA, COLOR_RED, COLOR_YELLOW);
   init_pair(COL_RANA_MARC, COLOR_RED, COLOR_GREEN);
   init_pair(COL_RANA_EV_MARC, COLOR_RED, COLOR_BLACK);
}

//funzione stampa tane
void tane(){
   wattron(gamewin, COLOR_PAIR(COL_MARCIAPIEDE));
   for(int i = 0; i < TANEY; i++) {
      for(int j = 0; j < TANEX; j++) {
         mvwprintw(gamewin, TANESY + i, TANESX + j, " ");
      }
   }
   wattroff(gamewin, COLOR_PAIR(COL_MARCIAPIEDE));
}

//funzione stampa riva
void riva() {
   wattron(gamewin, COLOR_PAIR(COL_RIVA));
   for(int i = 0; i < RIVA_Y; i++) {
      for(int j = 0; j < RIVA_X; j++) {
         mvwprintw(gamewin, RIVASY + i, RIVASX + j, " ");
      }
   }
   wattroff(gamewin, COLOR_PAIR(COL_RIVA));
}
//funzione stampa fiume
void fiume() {
   wattron(gamewin, COLOR_PAIR(COL_FIUME));
   for(int i = 0; i < FIUME_Y; i++) {
      for(int j = 0; j < FIUME_X; j++) {
         mvwprintw(gamewin, FIUMESY + i, FIUMESX + j, " ");
      }
   }
   wattroff(gamewin, COLOR_PAIR(COL_FIUME));
}
//funzione stampa marciapiede
void marciapiede(){
   wattron(gamewin, COLOR_PAIR(COL_MARCIAPIEDE));
   for(int i = 0; i < MARCIAPIEDE_Y; i++) {
      for(int j = 0; j < MARCIAPIEDE_X; j++) {
         mvwprintw(gamewin, MARCIAPIEDESY + i, MARCIAPIEDESX + j, " ");
      }
   }
   wattroff(gamewin, COLOR_PAIR(COL_MARCIAPIEDE));
}

//funzione che stampa lo scenario di gioco
void background(){
   tane();
   riva();
   fiume();
   marciapiede();
   }
   
//ritorna le dimensioni attuali dello schermo   
struct screen get_screen_size() {
   struct screen maxxy;
   getmaxyx(stdscr, maxxy.y, maxxy.x);
   return maxxy;
   }

//controlla se le dimensioni schermo sono conformi
bool is_screen_size_ok() {
   bool flag = true;
   if (get_screen_size().x <= DIM_X || get_screen_size().y <= DIM_Y) {
       flag = false;
   }
   return flag;
 }
 
//funzione che si assicura la schermata abbia dimensione adatta
void screen_size_loop() {
   while (!is_screen_size_ok()) {
          mvprintw(get_screen_size().y/2, get_screen_size().x/2, AVVISO_DIM_SCHERMO);
          refresh();
       }
}
 
//funzione che ritorna le coordinate da dare a newwin per   inizializzare la finestra di gioco
struct screen generate_win_start() {
   struct screen upper_left_corner;
   
   upper_left_corner = get_screen_size();
   upper_left_corner.y = (upper_left_corner.y - DIM_Y) / 2;
   upper_left_corner.x = (upper_left_corner.x - DIM_X) / 2;
   
   return upper_left_corner;
}
//funziona che ritorna il tempo passato da quando è stato misurato start
long int gettime(long int start){
   struct timespec end; long int time_passed;
   clock_gettime(CLOCK_REALTIME, &end);
   time_passed = end.tv_sec - start;
   return time_passed;
}

//Controlla se la frog è caduta nel fiume
bool frog_in_water() {
   if ((sfrog.y < MARCIAPIEDESY && sfrog.y + FROG_Y > FIUMESY) && !(sfrog.on_croc)) {
      //partita.manche = false;
      //partita.loss = true;
      kill_process(sfrog.pid_frog);
      return true;
   }
   return false;
}
 
//controlla se il tempo è finito
bool time_is_over(long int start) {
  bool over = false;
  //start è il tempo passato fino all'inizio della manche
  if ((gettime(start)) >= TEMPO_MANCHE) {
     over = true;
            }
  return over;
}

//funziona che controlla le condizioni di fine manche
bool manche_is_over(long int tempo_inizio) {
   bool over = false;
   if (time_is_over(tempo_inizio)) {
      //partita.manche = false;
      //partita.loss = true;
      over = true;
   }
   
   if (frog_in_water()) {
      over = true;
   }
   return over;
}

//Controllo se la rana ha vinto la manche
bool frog_win() {
   return false;
}

//funzione che genera la barra del tempo
void time_bar(long int tempo_inizio) {
   long int timeleft, tempopassato;
   int six_factors;
   
   tempopassato = gettime(tempo_inizio);
   timeleft = TEMPO_MANCHE - tempopassato;
   
   //preparo la barra
   if (timeleft == 60) {
      wattron(gamewin, COLOR_PAIR(COL_TIME-3));
      for (int i = TIME_START; i < DIM_X - TIME_START; i++) {
         mvwprintw(gamewin, TIME_Y, i, "  ");    
      }
      wattroff(gamewin, COLOR_PAIR(COL_TIME-3));
   }
   //operazione per assicurare che la barra sia calibrata alla lunghezza del campo da gioco
   //questo perché 150 % 60 = 30
   else if (timeleft % 60 != 0)  {
      wattron(gamewin, COLOR_PAIR(COL_TIME-2));
      mvwprintw(gamewin, TIME_Y, DIM_X-tempopassato*2.5-1, "   ");    
      wattroff(gamewin, COLOR_PAIR(COL_TIME-2));
   }   
   }

void display_vite(int vite) {
   for (int i = 0; i < vite; i++) {
      mvwprintw(gamewin, VITEY, VITEX + i*2, SYMB_VITE);
   }
}

void display_score(int punteggio) {
   mvwprintw(gamewin, SCOREY, SCOREX, "%d", punteggio);
}

//inizializza la schermata di gioco
void create_game_win() {
   gamewin = newwin(DIM_Y, DIM_X, generate_win_start().y, generate_win_start().x);
   box(gamewin, 0, 0);
}

//grafiche non legate alla "logica" di gioco
void miscellaneous_graphics(long int tempo_inizio, int vite, int punteggio) {
   display_vite(vite);
   display_score(punteggio);
   time_bar(tempo_inizio);
}

//Disegna la rana
void draw_frog(int x, int y) {
   int color;
   if (y >= MARCIAPIEDESY && y < MARCIAPIEDESY+MARCIAPIEDE_Y){color = COL_RANA_MARC;}
   else if (y > FIUMESY && y < FIUMESY + FIUME_Y){color = COL_RANA_FIUME;}
   else if (y >= RIVASY && y < RIVASY + RIVA_Y){color = COL_RANA_RIVA;}
   if (sfrog.on_croc){
      color = COL_RANA_MARC; 
      if (game_matrix[x+1][y].id == EV_CROC_ID ||  game_matrix[x-1][y].id == EV_CROC_ID || game_matrix[x+2][y].id == EV_CROC_ID ||  game_matrix[x-2][y].id == EV_CROC_ID)
      {   color = COL_RANA_EV_MARC;}
      }
   wattron(gamewin, COLOR_PAIR(color));
   mvwprintw(gamewin, y, x-CROC_X-1, game_matrix[x][y].ch);
   wattroff(gamewin, COLOR_PAIR(color));
   }

//switch per chiamare le funzioni di disegno
void draw_loop() {
   for (size_t i = CROC_X; i < DIM_X+CROC_X; i++) {
      for (size_t j = 0; j < DIM_Y; j++) {
            switch(game_matrix[i][j].id) {
               case FROG_ID:
                  draw_frog(i, j);
                  break;
               case CROC_ID:
                  wattron(gamewin, COLOR_PAIR(COL_TIME-7));
                  mvwprintw(gamewin, j, i-CROC_X-1, game_matrix[i][j].ch);
                  wattroff(gamewin, COLOR_PAIR(COL_TIME-7));
                  break;
               case EV_CROC_ID:
                  wattron(gamewin, COLOR_PAIR(COL_EV_CROC));
                  mvwprintw(gamewin, j, i-CROC_X-1, game_matrix[i][j].ch);
                  wattroff(gamewin, COLOR_PAIR(COL_EV_CROC)); 
                  break;
               case BULL_ID:
                  wattron(gamewin, COLOR_PAIR(COL_PR_RANA));
                  mvwprintw(gamewin, j, i-CROC_X-1, game_matrix[i][j].ch);
                  wattroff(gamewin, COLOR_PAIR(COL_PR_RANA));
                  break;
               case BULL_PL_ID:
                  wattron(gamewin, COLOR_PAIR(COL_PR_RANA));
                  mvwprintw(gamewin, j, i-CROC_X-1, game_matrix[i][j].ch);
                  wattroff(gamewin, COLOR_PAIR(COL_PR_RANA));
                  break;
               case PLANT_ID:
                  wattron(gamewin, COLOR_PAIR(COL_TIME-7));
                  mvwprintw(gamewin, j, i-CROC_X-1, game_matrix[i][j].ch);
                  wattroff(gamewin, COLOR_PAIR(COL_TIME-7));
                  break;
               default:
                  break;
            }
      }
   }
}

//funzione che chiama tutte le funzioni legate alla grafica
void generate_graphics(long int tempo_inizio, int vite, int punteggio) {
   background();
   miscellaneous_graphics(tempo_inizio, vite, punteggio);
   draw_loop();
   }
 
//funzione che gestisce il segnale mandato agli oggetti pianta (proiettile rana colpisce coccodrillo cattivo)  
void handle_signal_plants(int signal) {
    if (signal == SIGUSR1) {
       srand(getpid());
       usleep(rand() % ((PLANT_STOP_UPPER - PLANT_STOP_LOWER + 1) + PLANT_STOP_LOWER));  
    }
}
   
//funzione che gestisce il segnale mandato agli oggetti pianta  (proiettile rana colpisce pianta)
void handle_signal_crocs(int signal) {
    if (signal == SIGUSR1 && evil_flag == 0) {
       evil_flag = 1;
    }
}
   
//funzione pianta
void plant(int n) {
   struct msg m; int shoot;
   m.x_speed = 0;
   m.y_speed = 0;
   srand(getpid());
   m.x = CROC_X+PLANT_PX+PLANT_DISTANCE*n;
   m.y = FIRST_PLANT_PY;
   m.pid = getpid();
   m.id = PLANT_ID;
   while(true) {
      write(pipe_fds[1], &m, sizeof(m));
      //il segnale mi dice se la pianta è stata colpita da un proiettile rana
      signal(SIGUSR1, handle_signal_plants);
      shoot = rand() % 5;
      if (shoot == 3) {
         generate_process(BULL_PL_ID, -1, m.x);   
      }
      usleep(PLANT_DELAY);    
      }  
}

//funzione coccodrillo
void croc(int flusso, bool evil) {
   struct msg m;
   srand(getpid());
   m.x_speed = (flussi[flusso] < 0) ? (-1): (1);;
   //if (flusso == 3) {beep(); }
   m.y_speed = 0;
   m.id = (evil) ? (EV_CROC_ID): (CROC_ID);
   m.x = (m.x_speed < 0) ? (FIUMESX+FIUME_X+CROC_X): (0);
   m.y = ((flusso)*FLUX_Y)+(TANEY+RIVA_Y+3);
   m.pid = getpid();
   while(true) {
    if (evil_flag == 1) {m.id = CROC_ID;}
    if (m.x > -1 || m.x < CROC_X+2*FROG_X+1){
       if (m.id == EV_CROC_ID && (rand() % IMMERSION == 0)) {
          m.id = IMM_CROC_ID;
        }
       write(pipe_fds[1], &m, sizeof(m));
      }
    signal(SIGUSR1, handle_signal_crocs);
    //velocità flusso
    usleep(abs(flussi[flusso]));
    m.x += m.x_speed;  
   } 
}

//padre delle piante
void plant_creator() {
     struct msg m;
     m.id = FPLANT_ID;
     m.pid = getpid();
     write(pipe_fds[1], &m, sizeof(m));
     for (int i = 0; i < N_PIANTE; i++) {
        generate_process(PLANT_ID, -1, i);   
        }
     while (true) {
        
     }
}

//padre dei coccodrilli
void croc_creator() {
   srand(getpid());
   int flusso, old_flusso = N_FLUSSI+2, old_flusso1 = N_FLUSSI+3, pid_count = 0, id = CROC_ID;
   struct msg m;
   m.id = FCROC_ID;
   m.pid = getpid();
   write(pipe_fds[1], &m, sizeof(m));
   while(true) {
      flusso = rand() % N_FLUSSI;
      //mi assicuro il flusso sia libero
      while (flusso == old_flusso || flusso == old_flusso1) {
         flusso = rand() % (N_FLUSSI);
      }
      old_flusso = flusso;
      old_flusso1 = old_flusso;
      //generazione coccodrilli malvagi
      if (rand() % EV_CROC_RATE == 0) {
         id = EV_CROC_ID;
      }
      generate_process(id, flusso, -1);
      id = CROC_ID;
      pid_count += 1;
      //wait non bloccanti
      for (int i = 0; i <= pid_count; i++) {
         waitpid(-1, NULL, WNOHANG);
         }
      usleep(CROC_DELAY);  
    }   
}
//funzione del proiettile
void bullet(int x, int y, int flag) {
   struct msg m;
   m.x_speed = 0;
   m.y_speed = BULLET_SPEED*flag;
   m.id = (flag < 0) ? (BULL_ID) : (BULL_PL_ID);
   m.x = x;
   m.y = y;
   m.pid = getpid();
   while(true) {
      if (m.y > 0 && m.y < DIM_Y-2) {
         write(pipe_fds[1], &m, sizeof(m));
      }
      usleep(BULL_SPEED_DELAY);
      m.y += m.y_speed;  
      m.x = x;
      }
}

//funzione che controlla la rana
void frog() {
    
    struct msg m; pid_t pid; int input; bool new_input = false;
    
    m.id = FROG_ID;
    m.pid = getpid();
    //IMPORTANTE!!! Chiudo lo stream di output di questo processo; la ragione per cui è necessario è che chiamando getch() si chiama anche refresh()
    //questo da problemi di coerenza (o quantomeno non garantisce una esecuzione corretta) in alcuni casi.
    int fd = open("/dev/null", O_RDONLY);
    dup2(fd, STDOUT_FILENO);
    sfrog.x = FROG_STARTX;
    sfrog.y = FROG_STARTY;
    m.x = 0;
    m.y = 0;
    while(true) {
       input = (int)getch();
       
       //Controllo se c'è dell'input nello stream
       if (input != ERR) {new_input = true; } 
       else {input = false; new_input = false;}
       
        switch (input) {
        
            case KEY_UP:
                m.x = 0;
                m.y = -FROG_Y; // Moving up
                sfrog.x += m.x;
                sfrog.y += m.y;
                break;
                
            case KEY_DOWN:
                m.x = 0;
                m.y = FROG_Y; // Moving down
                sfrog.x += m.x;
                sfrog.y += m.y;
                break;
                
            case KEY_LEFT:
                m.x = -FROG_X;
                m.y = 0; // Moving left
                sfrog.x += m.x;
                sfrog.y += m.y;
                break;
                
            case KEY_RIGHT:
                m.x = FROG_X;
                m.y = 0; // Moving right
                sfrog.x += m.x;
                sfrog.y += m.y;
                break;
                
            case TAB:
               generate_process(BULL_ID, -1, -1);
               break;
               
            default:
               break;
        }
        if ((new_input) && input != TAB && (m.x != 0 || m.y != 0)) {
           write(pipe_fds[1], &m, sizeof(m));
        }
        
        else {m.x = 0; m.y = 0; write(pipe_fds[1], &m, sizeof(m));}
      usleep(10000);
    }
}

//template per generare processi
void generate_process(int id, int flusso, int n_plant) {
   pid_t pid;
   
   pid = fork();
   if (pid < 0) {perror("fork call"); _exit(2);}
   if (pid == 0) {
      close(pipe_fds[0]);
      switch(id) {
      
         case FROG_ID:
            frog();
            break;
            
         case FCROC_ID:
            croc_creator();
            break;
            
         case FPLANT_ID:
            plant_creator();
            break;
            
         case CROC_ID:
            croc(flusso, false);
            break;
         
         case EV_CROC_ID:
           croc(flusso, true);
           break;
            
         case PLANT_ID:
            plant(n_plant);
            break;
            
         case BULL_ID:
            bullet(sfrog.x+1, sfrog.y-BULL_Y, -1);  
            break;
            
         case BULL_PL_ID:
            bullet(n_plant, FIRST_PLANT_PY+PLANT_Y, 1);
            
         default:
            break;
      }
      _exit(2);
   }
   else {} // non che serva, era solo comodo "concettualmente"
}

//genera il processo rana
void generate_frog() {
   generate_process(FROG_ID, -1, -1);
}

//genera il processo che genererà i processi coccodrillo
void generate_crocs() {
   generate_process(FCROC_ID, -1, -1);
}

//genera il processo che genererà i processi pianta
void generate_plants() {
   generate_process(FPLANT_ID, -1, -1);
}

//funzione che genera i processi
void generate_processes() {
   generate_crocs();
   generate_frog();
   generate_plants();
}

//Uccide ogni processo presente in gioco (tranne se stesso)
void kill_all() {
   for (size_t i = 0; i < CROC_X*2 + DIM_X; i++) {
      for (size_t j = 0; j < DIM_Y; j++) {
         kill_process(game_matrix[i][j].pid);
      }
   }
   kill_process(fcroc_pid);
   kill_process(fplant_pid);
}
//Mando un segnale: lo uso per avvisare le piante di non mandare messaggi per un certo intervallo di tempo
void send_signal(pid_t pid) {
   kill(pid, SIGUSR1);
}


//pausa il processo: utilizzata quando le piante vengono colpite da un proiettile
void pause_process(pid_t pid) {
   kill(pid, SIGSTOP);
   //aspetto che il processo sia in pausa
   waitpid(pid, NULL, WUNTRACED);  
}

//Uccide il processo
void kill_process(pid_t pid)  {
   if (pid > 0) {
      kill(pid, SIGKILL);
      waitpid(pid, NULL, WNOHANG);
   }
}

//Funzione che ritorna il le coordinate della pianta più vicina
int which_plant(int x) {
   if (x <= CROC_X + PLANT_PX + PLANT_X) {
      return CROC_X + PLANT_PX;
     }
   else if (x < CROC_X + PLANT_PX + PLANT_X + PLANT_DISTANCE) {
      return CROC_X + PLANT_PX + PLANT_DISTANCE;
   }
   
   return CROC_X + PLANT_PX + PLANT_DISTANCE*2;

}

//controlla le collisioni prioettile-proiettile, proiettile-rana, proiettile-pianta e proiettile-coccodrillo
bool check_bull_collisions(pid_t pid, int x, int y, int speed) {
   bool flag = false; struct msg m;
   if (speed > 0) {
      if (game_matrix[x][y+BULL_Y-1].id == BULL_ID) {
         kill_process(pid);
         kill_process(game_matrix[x][y+BULL_Y-1].pid);
         m.x = x;
         m.id = BULL_ID;
         m.y = y;
         delete_old(entity_data[BULL_ID], m);
         m.y = y+speed;
         m.id = BULL_PL_ID;
         delete_old(entity_data[BULL_ID], m);
         m.y = y+2*speed;
         m.id = BULL_ID;
         delete_old(entity_data[BULL_ID], m);
         flag = true;
      }
      else if (game_matrix[x][y+BULL_Y].id == FROG_ID) {
         kill_process(pid);
         //kill_process(sfrog.pid_frog);
         m.x = x;
         m.y = y+2*speed;
         m.id = BULL_PL_ID;
         delete_old(entity_data[BULL_ID], m);
         partita.manche = false;
         //partita.loss = true;
         flag = true;
   }
   }
   else if (speed < 0) {
      if (game_matrix[x][y].id == BULL_PL_ID) {
         kill_process(pid);
         kill_process(game_matrix[x][y].pid);
         m.x = x;
         m.id = BULL_ID;
         m.y = y;
         delete_old(entity_data[BULL_ID], m);
         m.y = y+speed;
         m.id = BULL_ID;
         delete_old(entity_data[BULL_ID], m);
         m.y = y+2*speed;
         m.id = BULL_PL_ID;
         delete_old(entity_data[BULL_ID], m);
         flag = true;
      }
      else if (game_matrix[x][y].id == PLANT_ID) {
         kill_process(pid);
         send_signal(game_matrix[x][y].pid);
         m.x = x;
         m.y = y+speed;
         m.id = BULL_ID;
         delete_old(entity_data[BULL_ID], m);
         m.y = FIRST_PLANT_PY;
         m.x = which_plant(m.x);
         m.id = PLANT_ID;
         delete_old(entity_data[PLANT_ID], m);
         flag = true;
      }
      else if (game_matrix[x][y].id == CROC_ID) {
         kill_process(pid);
         m.x = x;
         m.y = y+speed;
         m.id = BULL_ID;
         delete_old(entity_data[BULL_ID], m);
         flag = true;
      }
     else if (game_matrix[x][y].id == EV_CROC_ID) {
         kill_process(pid);
         send_signal(game_matrix[x][y].pid);
         m.x = x;
         m.y = y+speed;
         m.id = BULL_ID;
         delete_old(entity_data[BULL_ID], m);
         flag = true;
      }
   
   }
   return flag;
}

//Controlla se ci sono state collisioni
bool check_collision(int x, int y, int speed) { 
  if (game_matrix[x][y].id == FROG_ID) {
     //sfrog.x += speed;
     return true;
  }
  return false;
}

//Controlla se la frog è sul croc
bool is_frog_on_croc() {
   pid_t old = game_matrix[sfrog.x][sfrog.y].pid; pid_t new = game_matrix[sfrog.x][sfrog.y].pid;
   for (size_t i = sfrog.y; i < sfrog.y+FROG_Y; i++) {
   for (size_t j = sfrog.x; j < sfrog.x+FROG_X; j++) { 
   old = new; new = game_matrix[j][i].pid;
   if (game_matrix[j][i].id != CROC_ID && game_matrix[j][i].id != EV_CROC_ID) {return false;}
   }
   }
   return true;
      
}

//Controlla se le coordinate sono lecite
bool is_out_of_bounds(struct msg m) {
  
   if (m.id == FROG_ID && (sfrog.y > MARCIAPIEDESY + MARCIAPIEDE_Y - FROG_Y || sfrog.x < CROC_X+1 || sfrog.x > FIUMESX + FIUME_X + CROC_X-FROG_X-2)) {
      partita.manche = false;
      //partita.loss = true;
      kill_process(sfrog.pid);
      return true;      
   }
   
   else if ((m.id == CROC_ID || m.id == EV_CROC_ID) && m.x_speed > 0 && m.x > (DIM_X + 2*CROC_X)) {
      return true;
   }
   
   else if ((m.id == CROC_ID || m.id == EV_CROC_ID) && m.x_speed < 0 && m.x < 0) {
      
      return true;
   }
   
   else if ((m.id == BULL_ID || m.id == BULL_PL_ID) && (m.y < TANESY || m.y > DIM_Y-6)) {
      delete_old(entity_data[m.id], m);
      return true;
   }
   
   return false;
}

//aggiorna i dati relativi alla rana
void update_frog(int x, int y, pid_t pid) {
   sfrog.x += x;
   sfrog.y += y;
}

//cancella le vecchie posizioni
void delete_old(struct msg e, struct msg m) {

   int old_x, old_y;
   if (m.id == FROG_ID) {
      old_x = sfrog.x;
      old_y = sfrog.y;
      old_x = old_x-m.x;
      old_y = old_y-m.y;
   }
   else {
      old_x = m.x-m.x_speed;
      old_y = m.y-m.y_speed;
      }   
   if (m.id == FROG_ID) {
   for (size_t i = old_y; i < old_y+e.y; i++) {
      for(size_t j = old_x; (j <old_x+e.x) && (j < DIM_X + CROC_X); j++) {
         game_matrix[j][i].pid = -1;
         game_matrix[j][i].id = -1;
         game_matrix[j][i].first = false;
      }
   }}
   
   else if ((m.id == CROC_ID || m.id == EV_CROC_ID  || m.id == IMM_CROC_ID) && m.x_speed > 0) {
     game_matrix[old_x][m.y].first = false;
     for (size_t i = old_y; i < old_y+CROC_Y; i++)
    {
      for(size_t j = old_x; (j <old_x+m.x_speed) && (j < DIM_X + 2*CROC_X); j++) {
         if (m.id == IMM_CROC_ID && game_matrix[j][i].id == FROG_ID) {sfrog.on_croc = false;}
         if (game_matrix[j][i].id == CROC_ID || game_matrix[j][i].id == EV_CROC_ID) {
         game_matrix[j][i].pid = -1;
         game_matrix[j][i].id = -1;
         game_matrix[j][i].first = false;
         }
      }
   }}
   else if ((m.id == CROC_ID || m.id == EV_CROC_ID  || m.id == IMM_CROC_ID) && m.x_speed < 0) {
      game_matrix[old_x][m.y].first = false;
      for (size_t i = old_y; i < old_y+CROC_Y; i++)
    {
      for(size_t j = CROC_X+m.x; (j <CROC_X+old_x); j++) {
      if (m.id == IMM_CROC_ID && game_matrix[j][i].id == FROG_ID) {sfrog.on_croc = false;}
      if (game_matrix[j][i].id == CROC_ID || game_matrix[j][i].id == EV_CROC_ID) {
         game_matrix[j][i].pid = -1;
         game_matrix[j][i].id = -1;
         game_matrix[j][i].first = false;
         }
      }
   }
   }
   else if (m.id == BULL_ID || m.id == BULL_PL_ID) {
      for(size_t i = old_y; i < old_y+BULL_Y; i++) {
        if (game_matrix[m.x][i].id == BULL_ID || game_matrix[m.x][i].id == BULL_PL_ID) {
          game_matrix[m.x][i].pid = -1;
          game_matrix[m.x][i].id = -1;
         }
      }
   }
  else if (m.id == PLANT_ID) {
     for (size_t i = m.y; i < m.y+PLANT_Y; i++) {
         for(size_t j = m.x; j < m.x+PLANT_X; j++) {
            game_matrix[j][i].pid = -1;
            game_matrix[j][i].id = -1;
      }
   }
  }
}

//aggiorna game_matrix
void update(struct msg e, struct msg m){
   struct msg old_m = m; int first_x, first_y;
   if (m.id == FROG_ID && (m.x != 0 || m.y != 0)) {
      update_frog(m.x, m.y, m.pid);
      m.x = sfrog.x;
      m.y = sfrog.y;
   }
   
   first_x = m.x;
   first_y = m.y;
   
   if (m.id == FROG_ID && (m.x != 0 || m.y != 0)) {sfrog.on_croc = is_frog_on_croc(); 
      if (sfrog.on_croc) {sfrog.pid = game_matrix[sfrog.x][sfrog.y+2].pid; mvwprintw(gamewin, 40, 80, "%d %d", sfrog.pid, game_matrix[sfrog.x][sfrog.y].pid);}}
   
   
   if (!sfrog.on_croc) {sfrog.pid = -2;}
   
   if (m.id == BULL_ID || m.id == BULL_PL_ID) {
      //if (m.pid < 0) {beep();}
      if (!check_bull_collisions(m.pid, m.x, m.y, m.y_speed)) {
      for (size_t i = m.y; i < m.y+2; i++) {
      for(size_t j = m.x; j < m.x+1; j++) {
         game_matrix[j][i].pid = m.pid;
         game_matrix[j][i].id = m.id;
         strcpy(game_matrix[j][i].ch, " ");
      }
   } }delete_old(e, old_m); }

   if (m.id == FROG_ID && (m.x != 0 || m.y != 0)) {
   sfrog.pid_frog = m.pid;
   game_matrix[m.x][m.y].first = true;
   for (size_t i = m.y; i < m.y+e.y; i++) {
      for(size_t j = m.x; j < m.x+e.x; j++) {
         game_matrix[j][i].pid = m.pid;
         game_matrix[j][i].id = m.id;
         strcpy(game_matrix[j][i].ch, frog_sprite[j-first_x+((FROG_X)*(i-first_y))]);
      }
   }delete_old(e, old_m);}
   if (m.id == PLANT_ID) {
      for (size_t i = m.y; i < m.y+PLANT_Y; i++) {
         for(size_t j = m.x; j < m.x+PLANT_X; j++) {
            game_matrix[j][i].pid = m.pid;
            game_matrix[j][i].id = m.id;
            strcpy(game_matrix[j][i].ch, plant_sprite[j-m.x+((PLANT_X)*(i-m.y))]);
      }
   }
   }
   if ((m.id == CROC_ID || m.id == EV_CROC_ID) && m.x_speed > 0 && m.x <= (DIM_X + 2*CROC_X)) {
      game_matrix[m.x][m.y].first = true;
      //if (m.id == EV_CROC_ID) {beep();}
      //((j < m.x+e.x) && (j < (DIM_X)))
      for (size_t i = m.y; i < m.y+CROC_Y; i++) {
      for(size_t j = m.x; (j < m.x+CROC_X) && (j < DIM_X + CROC_X); j++) {
         game_matrix[j][i].pid = m.pid;
         game_matrix[j][i].id = m.id;
         strcpy(game_matrix[j][i].ch, croc_sprite1[j-first_x+((CROC_X)*(i-first_y))]);
         }
      }
      delete_old(e, old_m);
      }
   if ((m.id == CROC_ID || m.id == EV_CROC_ID) && m.x_speed < 0 && m.x >= 0) {
       for (size_t i = m.y; i < m.y+CROC_Y; i++) {
          for(size_t j = m.x; j < m.x+CROC_X; j++) {
            game_matrix[j][i].pid = m.pid;
            game_matrix[j][i].id = m.id;
            strcpy(game_matrix[j][i].ch, croc_sprite[j-first_x+((CROC_X)*(i-first_y))]);
       }
      }   
      delete_old(e, old_m);
   }
   
   if ((sfrog.x >= m.x-m.x_speed && sfrog.x <= m.x-m.x_speed-FROG_X+CROC_X) && (sfrog.y >= m.y && sfrog.y < m.y + CROC_Y)) {
      sfrog.x += m.x_speed;
      for (size_t i = sfrog.y; i < sfrog.y+FROG_Y; i++) {
      for(size_t j = sfrog.x; j < sfrog.x+FROG_X; j++) {
         game_matrix[j][i].pid = 2;
         game_matrix[j][i].id = FROG_ID;
         strcpy(game_matrix[j][i].ch, frog_sprite[j-sfrog.x+((FROG_X)*(i-sfrog.y))]);
       }
     }
   }
   
}

//funzione per gestire i messaggi dalle pipe
void receive_data() {
   struct msg temp; 
   read(pipe_fds[0], &temp, sizeof(temp));
   //IMPORTANTE!!! questo check è necessario. Le read della pipe possono anche dare dati non fruibili.
   if (temp.id == CROC_ID || temp.id == EV_CROC_ID || temp.id == FROG_ID || temp.id == BULL_ID || temp.id == PLANT_ID || temp.id == BULL_PL_ID) {
      //controlla se gli oggetti sono nei limiti dell'array
      if (is_out_of_bounds(temp)) {
         kill_process(temp.pid);
      }
      else {
      update(entity_data[temp.id], temp);
      }
   }
   else if (temp.id == FCROC_ID) {
      fcroc_pid = temp.pid;
   }
   else if (temp.id == FPLANT_ID) {
      fplant_pid = temp.pid;
   }
   //Uccido e cancello il coccodrillo immerso: non c'è ragione per passare attraverso update.
   else if (temp.id == IMM_CROC_ID) {
      //è più comodo se lo cambio con un id effettivamente usato invece di aggiungere un check in delete_old
      temp.id = EV_CROC_ID;
      if (is_out_of_bounds(temp)) {
         kill_process(temp.pid);
      }
      temp.id = IMM_CROC_ID;
      //cancella senza disegnare (animazione di scomparsa del coccodrillo);
      delete_old(entity_data[CROC_ID], temp);
   }
}

//inizializzo le velocità dei flussi
void init_fluxes() {
   srand(time(NULL));
   for (int i = 0; i < N_FLUSSI; i++) {
      //range del rand
      flussi[i] = (rand() % (LOWER_CROC_SPEED + 1 - UPPER_CROC_SPEED)) + UPPER_CROC_SPEED;
      if (rand() % 2 == 0) {flussi[i] = flussi[i]*(-1);
      }
   }
}

//Inizializzo la pipe
void init_pipe() {
   if (pipe(pipe_fds) == -1) {
        perror("Pipe call");
        exit(1);
    }
}

//setto la posizione iniziale della rana
void ready_frog() {
   //game_matrix[FROG_STARTX][FROG_STARTY].id = 0;
   //game_matrix[FROG_STARTX][FROG_STARTY].pid = 0;
    sfrog.x = FROG_STARTX;
    sfrog.y = FROG_STARTY;
    sfrog.pid = -2;
    sfrog.on_croc = false;
}

//setto la game_matrix con le condizioni iniziali
void ready_game_matrix() {
   //setto la posizione iniziale della rana
   ready_frog();
   //preparo la matrice con i valori iniziali corretti
   for (size_t i = 0; i < DIM_X+CROC_X*2; i++) {
      for (size_t j = 0; j < DIM_Y; j++) {
         game_matrix[i][j].id = -1;
         game_matrix[i][j].pid = -1; 
         game_matrix[i][j].first = false;
      }
   }
   //setto la posizione iniziale della rana nella matrice
   for (size_t i = sfrog.y; i < sfrog.y+FROG_Y; i++) {
      for(size_t j = sfrog.x; j < sfrog.x+FROG_X; j++) {
         game_matrix[j][i].pid = -1;
         game_matrix[j][i].id = FROG_ID;
         strcpy(game_matrix[j][i].ch, frog_sprite[j-sfrog.x+((FROG_X)*(i-sfrog.y))]);
      }
   }
}

//funzione che setta entity_data con le generalità delle entità
void ready_entity_data() {
   entity_data[FROG_ID].x = FROG_X;
   entity_data[FROG_ID].y = FROG_Y;
   entity_data[CROC_ID].x = CROC_X;
   entity_data[CROC_ID].y = CROC_Y;
   entity_data[EV_CROC_ID].x = CROC_X;
   entity_data[EV_CROC_ID].y = CROC_Y;
}

//aggiorna le vite, il punteggio e controlla che la run possa continuare
void clean_run(struct manchestr partita, struct match_data* dati_finali) {

   if (partita.loss) {
         //dati_finali->score += partita.score;
         dati_finali->vite -= 1;
      }
      
   if (dati_finali->vite <= 0) {
         dati_finali->running = false;
      }
}

//funzione che contiene il loop della manche
struct manchestr game(bool manche, long int startingtime, int vite, int score) {

   partita.loss = false;
   partita.manche = manche;
   while(partita.manche) {
      //controllo dimensioni schermo
      screen_size_loop();
      //Ricevo i dati e li gestico (collisioni ecc...)
      receive_data();
      //grafica
      generate_graphics(startingtime, vite, score);
      //controllo condizioni di break
      if (manche_is_over(startingtime) && frog_win() != true) {
         partita.manche = false;
         //partita.loss = true;
         partita.score = score;
         //kill_process(sfrog.pid_frog);
      }
      //update schermo
      wrefresh(gamewin);
   }
   //uccido tutti i processi
   kill_all();
   //ritorna true se la manche è stata persa
   return partita;
}

//funzione loop di gioco
void run(bool running, bool manche, struct timespec start, int vite, int score) {
   
   struct manchestr partita; struct match_data game_data;
   game_data.running = true; game_data.vite = VITE;
   while(game_data.running) {
      //controllo la size dello shermo
      screen_size_loop();
      //ogni ciclo resetto le condizioni del loop
      manche = true;
      //inizializzo la pipe
      //il motivo per cui la inizializzo qui e non in game_polling è perché
      //voglio che venga rinizializzata prima di ogni manche (non posso riaprire la pipe ma posso ricrearla)
      init_pipe();
      //misuro il tempo di inizio
      clock_gettime(CLOCK_REALTIME, &start);
      //pulisco la finestra e ridisegno il box
      wclear(gamewin);
      box(gamewin, 0, 0);
      //inizializzo le velocità dei flussi
      init_fluxes();
      //genero i processi
      generate_processes();
      //sono in lettura
      close(pipe_fds[1]);
      //setto i valori iniziali di game_matrix
      ready_game_matrix();
      //loop della manche
      partita = game(manche, start.tv_sec, vite, score); 
      //aggiorno i dati della run
      clean_run(partita, &game_data);
      }
}
 
//funzione che controlla la logica del gioco
void game_polling() {

   //dichiaro e inizializzo le condizioni e le variabili iniziali
   bool running = true, manche = true; struct timespec start; int vite = VITE, score = 0;
   ready_entity_data();
   //controllo che le dimensioni schermo vadano bene
   screen_size_loop();
   //IMPORTANTE!!! Il motivo per cui non inizializzo la finestra di gioco in game_init() è che
   //adesso ho la sicurezza che le coordinate usate per generare la finestra con generate_win_start() sono adatte allo schermo
   //grazie al while precedente
   create_game_win();
   //loop di gioco
   run(running, manche, start, vite, score);
 }

