#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <conio.h>
#include <windows.h>

HANDLE hStdout;
DWORD outdwMode;

#endif

#ifdef __linux__
#include <sys/time.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>

struct termios original_termios;
#endif

#define N 4
#define BOARD_W 10
#define BOARD_H 20
#define MAXBUFF 2048
#define INTERVAL_DECREMENT 25
#define MAX(A,B) A > B ? A : B

typedef struct Shape {
  char bits[N][N];
  size_t size;
} Shape_t;

const Shape_t shapes[] = {
  // I
  {
    {
      {0, 0, 0, 0},
      {1, 1, 1, 1},
      {0, 0, 0, 0},
      {0, 0, 0, 0}
    }, 4
  },
  // O
  {
    {
      {1, 1},
      {1, 1}
    }, 2
  },
  // J
  {
    {
      {1, 0, 0},
      {1, 1, 1},
      {0, 0, 0},
    }, 3
  },
  // L
  {
    {
      {0, 0, 1},
      {1, 1, 1},
      {0, 0, 0},
    }, 3
  },
  // S
  {
    {
      {0, 1, 1},
      {1, 1, 0},
      {0, 0, 0},
    }, 3
  },
  // Z
  {
    {
      {1, 1, 0},
      {0, 1, 1},
      {0, 0, 0},
    }, 3
  },
  // T
  {
    {
      {0, 1, 0},
      {1, 1, 1},
      {0, 0, 0},
    }, 3
  }
};

const int number_of_shapes = sizeof(shapes) / sizeof(Shape_t);

typedef struct CBuff {
  unsigned char bits[MAXBUFF];
  int len;
} CBuff_t;

typedef struct Tetris {
  int x;
  int y;
  int level;
  int score;
  int running;
  int interval;
  CBuff_t buff;
  char board[BOARD_H][BOARD_W];
  Shape_t shape;
  int shape_color;
  int append_shape;
  int board_updated;
  int next_shape_index;
  int next_shape_color;
} Tetris_t;

Tetris_t game;

struct timespec ctimespec, ptimespec;

void exit_error(const char *msg) {
  fprintf(stderr, "Error: %s.\n", msg);
  exit(1);
}

void swapi(int *a, int *b) {
  int temp = *a;
  *a = *b;
  *b = temp;
}

void swapc(char *a, char *b) {
  char temp = *a;
  *a = *b;
  *b = temp;
}

int get_random_color() {
  return 91 + rand() % 7;
}

int get_random_shape_index() {
  return rand() % number_of_shapes;
}

long long get_interval(struct timespec *start, struct timespec *end) {
  return (end->tv_sec - start->tv_sec) * 1000 + round((end->tv_nsec - start->tv_nsec) / 1e6);
} 

int move_shape_collides(Shape_t *shape, int x, int y) {
  for (int i = 0; i < shape->size; ++i) {
    for (int j = 0; j < shape->size; ++j) {
      if (shape->bits[i][j] != 0) {
        if (x + j < 0 || 
            x + j >= BOARD_W || 
            y + i >= BOARD_H ||
            game.board[y + i][x + j] != 0) 
        {
          return 1;
        }
      }
    }
  }
  return 0;
}

void rotate_shape() {
  int rotate = 1;
  Shape_t tempShape;
  memcpy(&tempShape, &game.shape, sizeof(Shape_t));

  for (int i = 0; i < tempShape.size; ++i) {
    for (int j = 0; j < i; ++j) {
      swapc(&tempShape.bits[i][j], &tempShape.bits[j][i]);
    }
  }
  for (int i = 0; i < tempShape.size; ++i) {
    for (int j = 0; j < tempShape.size / 2; ++j) {
      swapc(&tempShape.bits[i][j], &tempShape.bits[i][tempShape.size - j - 1]);
    }
  }

  if (move_shape_collides(&tempShape, game.x, game.y)) {
    rotate = 0;
    int attempts = tempShape.size / 2;
    for (int k = 1; k <= attempts; k++) {
      if (!move_shape_collides(&tempShape, game.x + k, game.y)) {
        rotate = 1;
        game.x += k;
        break;
      } else if (!move_shape_collides(&tempShape, game.x - k, game.y)) {
        rotate = 1;
        game.x -= k;
        break;
      }
    }
  }

  if (rotate) {
    memcpy(&game.shape, &tempShape, sizeof(Shape_t));
  }
}

void update_shape() {
  memcpy(&game.shape, &shapes[game.next_shape_index], sizeof(Shape_t));
  game.shape_color = game.next_shape_color;
  game.y = 0;
  game.x = BOARD_W / 2 - game.shape.size / 2;
  game.next_shape_index = get_random_shape_index();
  game.next_shape_color = get_random_color();
}

void update_board() {
  if (game.append_shape) {
    for (int i = 0; i < game.shape.size; ++i) {
      for (int j = 0; j < game.shape.size; ++j) {
        if (game.shape.bits[i][j]) {
          game.board[game.y + i][game.x + j] = game.shape_color;
        }
      }
    }
    if (game.y != 0) {
      int i = BOARD_H - 1;
      while (i >= 0) {
        int count = 0;
        for (int j = 0; j < BOARD_W; ++j) {
          if (game.board[i][j] != 0) {
            count++;
          }
        }
        if (count == BOARD_W) {
          memmove((char *)game.board + BOARD_W, game.board, BOARD_W * i);
          memset(game.board, 0, BOARD_W);
          game.score += BOARD_W;
          if (game.score >= game.level * 100) {
            game.level++;
            if (game.interval > 250) {
              game.interval -= INTERVAL_DECREMENT;
            }
          }
        } else if (count == 0) {
          break;
        } else {
          i--;
        }
      }
      update_shape();
      game.append_shape = 0;
    } else {
      game.running = 0;
    }
    game.board_updated = 1;
  }
  if (get_interval(&ptimespec, &ctimespec) >= game.interval) {
    memcpy(&ptimespec, &ctimespec, sizeof(struct timespec));
    if (!move_shape_collides(&game.shape, game.x, game.y + 1)) {
      game.y++;
      game.board_updated = 1;
    } else {
      game.append_shape = 1;
    }
  }
  timespec_get(&ctimespec, TIME_UTC);
}

void buffAppend(const char *s, int len) {
  if (game.buff.len + len > sizeof(game.buff.bits)) {
    fprintf(stderr, "No enough space in game's buffer\n");
    exit(1);
  }
  memcpy(&game.buff.bits[game.buff.len], s, len);
  game.buff.len += len;
}

#define RESET_CBUFF(buff) buff.len = 0;\
                          memset(buff.bits, 0, sizeof(buff.bits));

void draw_board() {
  if (!game.board_updated) {
    return;
  }
  game.board_updated = 0;
  char c, buf[128], buf2[32];
  int color = 0, slen, midl, pad;
  RESET_CBUFF(game.buff);
  buffAppend("\x1b[H\xc9", 4);
  memset(buf, '\xcd', 20);
  buffAppend(buf, 20);
  buffAppend("\xcb", 1);
  memset(buf, '\xcd', 20);
  buffAppend(buf, 20);
  buffAppend("\xbb\r\n", 3);
  for (int i = 0; i < BOARD_H; ++i) {
    buffAppend("\xba", 1);
    for (int j = 0; j < BOARD_W; ++j) {
      if (j >= game.x && 
          i >= game.y && 
          j < game.x + game.shape.size && 
          i < game.y + game.shape.size && 
          game.shape.bits[i - game.y][j - game.x] != 0) 
      {
        if (color != game.shape_color) {
          color = game.shape_color;
          sprintf(buf, "\x1b[%dm", color);
          buffAppend(buf, 5);
        }
        buffAppend("\xdb\xdb", 2);
      } else {
        if (game.board[i][j]) {
          if (color != game.board[i][j]) {
            color = game.board[i][j];
            sprintf(buf, "\x1b[%dm", color);
            buffAppend(buf, 5);
          }
          buffAppend("\xdb\xdb", 2);
        } else {
          if (color != 0) {
            color = 0;
            buffAppend("\x1b[0m", 4);
          }
          buffAppend("  ", 2);
        }
      }
    }
    color = 0;
    buffAppend("\x1b[0m\xba", 5);
    if (i == 1) {
      buffAppend("       SCORE:       ", 20);
    } else if (i == 2) {
      slen = snprintf(buf2, sizeof(buf2), "%d", game.score);
      midl = (20 - slen) / 2;
      pad = midl + (slen % 2);
      memset(buf, ' ', pad);
      buffAppend(buf, pad);
      buffAppend(buf2, slen);
      pad = 20 - (pad + slen);
      memset(buf, ' ', pad);
      buffAppend(buf, pad);
    } else if (i == 4) {
      buffAppend("       LEVEL:       ", 20);
    } else if (i == 5) {
      slen = snprintf(buf2, sizeof(buf2), "%d", game.level);
      midl = (20 - slen) / 2;
      pad = midl + (slen % 2);
      memset(buf, ' ', pad);
      buffAppend(buf, pad);
      buffAppend(buf2, slen);
      pad = 20 - (pad + slen);
      memset(buf, ' ', pad);
      buffAppend(buf, pad);
    } else if (i == 10) {
      buffAppend("       NEXT:        ", 20);
    } else if (i >= 12 && i < 12 + shapes[game.next_shape_index].size) 
    {
      int ii = i - 12;
      pad = 10 - (shapes[game.next_shape_index].size);
      memset(buf2, ' ', pad);
      buffAppend(buf2, pad);
      sprintf(buf, "\x1b[%dm", game.next_shape_color);
      buffAppend(buf, 5);
      for (int k = 0; k < shapes[game.next_shape_index].size; ++k) {
        if (shapes[game.next_shape_index].bits[ii][k]) {
          buffAppend("\xdb\xdb", 2);
        } else {
          buffAppend("  ", 2);
        }
      }
      buffAppend("\x1b[0m", 4);
      buffAppend(buf2, pad);
    } else {
      buffAppend("                    ", 20);
    }
    buffAppend("\xba\x1b[K\r\n", 6);
  }
  buffAppend("\xc8", 1);
  memset(buf, '\xcd', 20);
  buffAppend(buf, 20);
  buffAppend("\xca", 1);
  memset(buf, '\xcd', 20);
  buffAppend(buf, 20);
  buffAppend("\xbc\r\n", 3);
  if (!game.running) {
    slen = snprintf(
      buf, 
      sizeof(buf), 
      "\x1b[%d;%dH\x1b[97m GAME OVER \r\n"
      "\x1b[3G Do you want to \r\n"
      "\x1b[3G play again? y/n \x1b[0m", 
      BOARD_H / 2, BOARD_W / 2);
    buffAppend(buf, slen);
  }
  fwrite(game.buff.bits, 1, game.buff.len, stdout);
}

void reset_game() {
  game.x = 0;
  game.y = 0;
  game.level = 1;
  game.score = 0;
  game.running = 1;
  game.interval = 1000;
  game.buff.len = 0;
  game.append_shape = 0;
  game.board_updated = 1;
  game.next_shape_index = get_random_shape_index();
  game.next_shape_color = get_random_color();
  memset(game.board, 0, BOARD_H * BOARD_W);
  update_shape();
}

int kbhitfix() {

  #ifdef _WIN32
  return _kbhit();
  #endif

  #ifdef __linux__
  int ofcntl = fcntl(STDIN_FILENO, F_GETFL, 0);
  fcntl(STDIN_FILENO, F_SETFL, ofcntl | O_NONBLOCK);
  int ch = getchar();
  fcntl(STDIN_FILENO, F_SETFL, ofcntl);
  if (ch != EOF) {
    ungetc(ch, stdin);
    return 1;
  }
  return 0;
  #endif
}

int getchfix() {

  #ifdef _WIN32
  return _getch();
  #endif

  #ifdef __linux__
  return getchar();
  #endif
}

void handle_input() {
  int c;
  if (!game.running) {
    while (1) {
      c = getchfix();
      if (c == 'y') {
        reset_game();
        return;
      } else if (c == 'n') {
        exit(0);
      }
    }
  }

  if (!kbhitfix()) {
    return;
  }
  c = getchfix();
  if (c == 224) {
    c = getchfix();
    switch (c) {
      case 72: // UP ARROW 
        rotate_shape();
        game.board_updated = 1;
        break;
      case 75: // LEFT ARROW
        if (!move_shape_collides(&game.shape, game.x - 1, game.y)) {
          game.x--;
          game.board_updated = 1;
        } 
        break;
      case 77: // RIGHT ARROW
        if (!move_shape_collides(&game.shape, game.x + 1, game.y)) {
          game.x++;
          game.board_updated = 1;
        }
        break;
      case 80:
        if (!move_shape_collides(&game.shape, game.x, game.y + 1)) {
          game.y++;
          game.board_updated = 1;
        } else {
          game.append_shape = 1;
        }
        break; // DOWN ARROW
    }
  } else {
    switch (c) {
      case 27: 
        exit(0); 
        break;
    }
  }
}

void exit_console(void) {
  #ifdef _WIN32
  if (hStdout != INVALID_HANDLE_VALUE && outdwMode) {
    printf("\x1b[!p");
    printf("\x1b[?25h");
    printf("\x1b[?1049l");
    SetConsoleMode(hStdout, outdwMode);
  }
  #endif

  #ifdef __linux__
  tcsetattr(STDIN_FILENO, TCSANOW, &original_termios);
  #endif
}

void init_console() {
  
  atexit(exit_console);

  #ifdef _WIN32
  DWORD dwMode;
  hStdout = GetStdHandle(STD_OUTPUT_HANDLE);

  if (hStdout == INVALID_HANDLE_VALUE) {
    exit_error("Invalid stdout handle value");
  }
  if (!GetConsoleMode(hStdout, &outdwMode)) {
    exit_error("Can't get stdout handle mode");
  }


  dwMode = outdwMode;
  dwMode |= ENABLE_PROCESSED_OUTPUT;
  dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;

  if (!SetConsoleMode(hStdout, dwMode)) {
    exit_error("Can't set stdout handle mode");
  }
  #endif

  #ifdef __linux__
  struct termios new_termios;
  tcgetattr(STDIN_FILENO, &original_termios);
  new_termios = original_termios;
  new_termios.c_lflag &= ~(ECHO | ICANON);
  tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);
  #endif
  
  printf("\x1b[?1049h");
  printf("\x1b]0;%s\x07", "Tetris in console");
  printf("\x1b[?25l");

  srand(time(NULL));
  timespec_get(&ctimespec, TIME_UTC);
  timespec_get(&ptimespec, TIME_UTC);
  reset_game();
}

void game_intro() {
  printf("  _____    _       _    \r\n"
         " |_   _|__| |_ _ _(_)___\r\n"
         "   | |/ -_)  _| '_| (_-<\r\n"
         "   |_|\\___|\\__|_| |_/__/\r\n\r\n");
  printf("Press any key to continue...");
  getchfix();
}

int main() {

  init_console();
  game_intro();

  while (1) {
    handle_input();
    update_board();
    draw_board();
  }

  return 0;
}
