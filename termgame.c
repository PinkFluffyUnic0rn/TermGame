#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <time.h>

#define MAX_ENEMY_COUNT 25
#define FIELD_WIDTH 40
#define FIELD_HEIGHT 20
#define TIMERSTEPMOVE 0.25
#define TIMERSTEPSPAWN 1.0

void terminategame(const char *msg, int retcode);

#define XPRINTF(...) if (printf(__VA_ARGS__) < 0) terminategame("Output error.", 1);

struct termios prevt;

struct object {
	int x;
	int y;

	char *model;
	int w;
	int h;
};

int ttysetraw(int fd, struct termios *prevterm)
{
	struct termios t;
	
	if (tcgetattr(0, &t) < 0)
		return 1;

	if (prevterm != NULL)	
		*prevterm = t;

	t.c_lflag &= ~(ICANON | ISIG | IEXTEN | ECHO);

	t.c_iflag &= ~(BRKINT | ICRNL | IGNBRK | IGNCR | INLCR
		| INPCK | ISTRIP | IXON | PARMRK);

	t.c_oflag &= ~OPOST;

	t.c_cc[VMIN] = 1;
	t.c_cc[VTIME] = 0;

	if (tcsetattr(0, TCSAFLUSH, &t) < 0)
		return 1;

	return 0;
}

void eraseobject(struct object *obj)
{
	int x, y;

	for (y = 0; y < obj->h; ++y)
		for (x = 0; x < obj->w; ++x) {
			XPRINTF("\e[%d;%dH", obj->y + y, obj->x + x);
			XPRINTF("%c", ' ');
		}
}

void drawobject(struct object *obj)
{
	int x, y;

	for (y = 0; y < obj->h; ++y)
		for (x = 0; x < obj->w; ++x) {
			XPRINTF("\e[%d;%dH", obj->y + y, obj->x + x);
			XPRINTF("%c", obj->model[y * obj->w + x]);
		}
}

int objectcollision(struct object *obj1, struct object *obj2)
{
	int x, y;

	for (y = obj1->y; y < obj1->y + obj1->h; ++y)
		for (x = obj1->x; x < obj1->x + obj1->w; ++x)
			if (x >= obj2->x && x < obj2->x + obj2->w
				&& y >= obj2->y && y < obj2->y + obj2->h)
				return 1;

	return 0;
}

void spawnenemy(struct object enemies[MAX_ENEMY_COUNT], int *enemycount)
{
	int r;
	
	if (*enemycount >= MAX_ENEMY_COUNT)
		return;

	r = rand() % 10;
	
	if (r >= 0 && r <= 6) {
		enemies[*enemycount].model = "++";
		enemies[*enemycount].w = 2;
		enemies[*enemycount].h = 1;
	}
	else if (r >= 7 && r <= 8) {
		enemies[*enemycount].model = "++++ ##  || ";
		enemies[*enemycount].w = 4;
		enemies[*enemycount].h = 3;
	}
	else {
		enemies[*enemycount].model = "   ||||   ##########|   ||   |   ####       \\/    ";
		enemies[*enemycount].w = 10;
		enemies[*enemycount].h = 5;
	}

	enemies[*enemycount].x = rand()
		% (FIELD_WIDTH - enemies[*enemycount].w) + 1;
	enemies[*enemycount].y = 1;


	++(*enemycount);
}

void initplayer(struct object *player)
{
	player->x = FIELD_WIDTH / 2;
	player->y = FIELD_HEIGHT;
	player->w = 1;
	player->h = 1;
	player->model = "H";

	drawobject(player);
}

void terminategame(const char *msg, int retcode)
{
	XPRINTF("\ec");
	XPRINTF("\e[1;1H");
	fflush(stdout);

	if (tcsetattr(0, TCSAFLUSH, &prevt) < 0)
		exit(1);

	XPRINTF("%s\n", msg);

	exit(retcode);
}

int keypress(char *c)
{
	fd_set fds;
	struct timeval tv;

	FD_ZERO(&fds);
	FD_SET(STDIN_FILENO,&fds);
	tv.tv_sec = 0;
	tv.tv_usec = 0;

	if (select(1, &fds, NULL, NULL, &tv) <= 0)
		return 0;

	if ((*c = getc(stdin)) == EOF)
		terminategame("Input error.", 1);

	tcflush(STDIN_FILENO, TCIFLUSH);

	return 1;	
}

int main()
{
	struct winsize ws;
	
	struct object player;
	struct object enemies[MAX_ENEMY_COUNT];
	int enemycount;

	clock_t tm, ts;

	if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) < 0)
		terminategame("Cannot get terminal size.", 1);
	
	if (ws.ws_col < FIELD_WIDTH || ws.ws_row < FIELD_HEIGHT) {
		char msg[1024];

		sprintf(msg, "Your terminal doesn't have needed size: \
your size is %dx%d, but %dx%d is need.\n",
			ws.ws_col, ws.ws_row, FIELD_WIDTH, FIELD_HEIGHT);
		
		terminategame(msg, 1);
	}

	ttysetraw(STDIN_FILENO, &prevt);

	XPRINTF("\ec\e[1;1H");
	XPRINTF("\e[?25l");
	if (fflush(stdout) < 0)
		terminategame("Output error.", 1);

	initplayer(&player);
	
	enemycount = 0;

	tm = ts = clock();
	while (1) {
		double td;
		clock_t tcur;

		char c;
		int i;
		
		if ((tcur = clock()) < 0)
			terminategame("Cannot get proccessor time.", 1);
		
		// periodic event: enemy spawn
		td = (tcur - ts) / (double) CLOCKS_PER_SEC;
		
		if (td > TIMERSTEPSPAWN) {
			if ((rand() % 2) == 0)
				spawnenemy(enemies, &enemycount);

			ts = tcur;
		}

		// periodic event: enemy move
		td = (tcur - tm) / (double) CLOCKS_PER_SEC;
		if (td > TIMERSTEPMOVE) {
			for (i = 0; i < enemycount; ++i) {
				eraseobject(enemies + i);
			
				enemies[i].y += 1;
			
				if (enemies[i].y + enemies[i].h - 1 > FIELD_HEIGHT) {
					enemies[i] = enemies[--enemycount];
					continue;
				}

				drawobject(enemies + i);
			}
			
			tm = tcur;
		}

		// player control handling
		if (keypress(&c)) {
			eraseobject(&player);

			switch (c) {
			case 'h':
				player.x -= 1;
				player.x = player.x >= 1 ? player.x : 1;
				break;
			case 'l':
				player.x += 1;
				player.x = player.x <= FIELD_WIDTH
					? player.x : FIELD_WIDTH;
				break;
			case 'k':
				player.y -= 1;
				player.y = player.y >= 1 ? player.y : 1;
				break;
			case 'j':
				player.y += 1;
				player.y = player.y <= FIELD_HEIGHT
					? player.y : FIELD_HEIGHT;
				break;
			case 27:
				terminategame("Game was ended by player.", 0);
			}
			
			drawobject(&player);
		}
		
		// if player collides enemy, game is over
		for (i = 0; i < enemycount; ++i)
			if (objectcollision(&player, enemies + i))
				terminategame("GAME OVER", 0);

		if (fflush(stdout) < 0)
			terminategame("Output error.", 1);
	}

	terminategame("Unknown error", 1);
}
