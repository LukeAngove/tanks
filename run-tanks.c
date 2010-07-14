#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "ctanks.h"
#include "cforf.h"
#include "dump.h"

#define MAX_TANKS 50
#define ROUNDS 500
#define SPACING 150

#define LENV_SIZE 100

#define DSTACK_SIZE 200
#define CSTACK_SIZE 500
#define MEMORY_SIZE 10

struct forftank {
  struct forf_env  env;
  char             color[7];    /* "ff0088" */
  char             name[50];

  struct forf_stack  _prog;
  struct forf_value  _progvals[CSTACK_SIZE];
  struct forf_stack  _cmd;
  struct forf_value  _cmdvals[CSTACK_SIZE];
  struct forf_stack  _data;
  struct forf_value  _datavals[DSTACK_SIZE];
  struct forf_memory _mem;
  long               _memvals[MEMORY_SIZE];
};


#ifndef NODEBUG
void
forf_print_val(struct forf_value *val)
{
  switch (val->type) {
    case forf_type_number:
      printf("%d", val->v.i);
      break;
    case forf_type_proc:
      printf("[proc %p]", val->v.p);
      break;
    case forf_type_stack_begin:
      printf("{");
      break;
    case forf_type_stack_end:
      printf("}");
      break;
  }
}

void
forf_print_stack(struct forf_stack *s)
{
  size_t pos;

  for (pos = 0; pos < s->top; pos += 1) {
    forf_print_val(&(s->stack[pos]));
    printf(" ");
  }
}

void
forf_dump_stack(struct forf_stack *s)
{
  printf("Stack at %p: ", s);
  forf_print_stack(s);
  printf("\n");
}
#endif

/*
 *
 * Forf API
 *
 */

/** Has the turret recharged? */
void
forf_tank_fire_ready(struct forf_env *env)
{
  struct tank *tank = (struct tank *)env->udata;

  forf_push_num(env, tank_fire_ready(tank));
}

/** Fire! */
void
forf_tank_fire(struct forf_env *env)
{
  struct tank *tank = (struct tank *)env->udata;

  tank_fire(tank);
}

/** Set desired speed */
void
forf_tank_set_speed(struct forf_env *env)
{
  struct tank *tank  = (struct tank *)env->udata;
  long         right = forf_pop_num(env);
  long         left  = forf_pop_num(env);

  tank_set_speed(tank, left, right);
}

/** Get the current turret angle */
void
forf_tank_get_turret(struct forf_env *env)
{
  struct tank *tank  = (struct tank *)env->udata;
  float        angle = tank_get_turret(tank);

  forf_push_num(env, rad2deg(angle));
}

/** Set the desired turret angle */
void
forf_tank_set_turret(struct forf_env *env)
{
  struct tank *tank  = (struct tank *)env->udata;
  long         angle = forf_pop_num(env);

  tank_set_turret(tank, deg2rad(angle));
}

/** Is a sensor active? */
void
forf_tank_get_sensor(struct forf_env *env)
{
  struct tank *tank       = (struct tank *)env->udata;
  long         sensor_num = forf_pop_num(env);

  forf_push_num(env, tank_get_sensor(tank, sensor_num));
}

/** Set the LED state */
void
forf_tank_set_led(struct forf_env *env)
{
  struct tank *tank   = (struct tank *)env->udata;
  long         active = forf_pop_num(env);

  tank_set_led(tank, active);
}

/** Pick a random number */
void
forf_proc_random(struct forf_env *env)
{
  long max = forf_pop_num(env);

  forf_push_num(env, rand() % max);
}

/* Tanks lexical environment */
struct forf_lexical_env tanks_lenv_addons[] = {
  {"fire-ready?", forf_tank_fire_ready},
  {"fire!", forf_tank_fire},
  {"set-speed!", forf_tank_set_speed},
  {"get-turret", forf_tank_get_turret},
  {"set-turret!", forf_tank_set_turret},
  {"sensor?", forf_tank_get_sensor},
  {"set-led!", forf_tank_set_led},
  {"random", forf_proc_random},
  {NULL, NULL}
};

/*
 *
 * Filesystem stuff
 *
 */

int
ft_read_file(char *ptr, size_t size, char *dir, char *fn)
{
  char  path[256];
  FILE *f       = NULL;
  int   ret;
  int   success = 0;

  do {
    snprintf(path, sizeof(path), "%s/%s", dir, fn);
    f = fopen(path, "r");
    if (! f) break;

    ret = fread(ptr, 1, size - 1, f);
    ptr[ret] = '\0';
    if (! ret) break;

    success = 1;
  } while (0);

  if (f) fclose(f);
  if (! success) {
    return 0;
  }
  return 1;
}

void
ft_bricked_tank(struct tank *tank, void *ignored)
{
  /* Do nothing, the tank is comatose */
}

void
ft_run_tank(struct tank *tank, struct forftank *ftank)
{
  int ret;

  /* Copy program into command stack */
  forf_stack_copy(&ftank->_cmd, &ftank->_prog);
  forf_stack_reset(&ftank->_data);
  ret = forf_eval(&ftank->env);
  if (! ret) {
    fprintf(stderr, "Error in %s: %s\n",
            ftank->name,
            forf_error_str[ftank->env.error]);
  }
}

int
ft_read_program(struct forftank         *ftank,
                struct tank             *tank,
                struct forf_lexical_env *lenv,
                char                    *path)
{
  int   ret;
  char  progpath[256];
  FILE *f;

  /* Open program */
  snprintf(progpath, sizeof(progpath), "%s/program", path);
  f = fopen(progpath, "r");
  if (! f) return 0;

  /* Parse program */
  ret = forf_parse_file(&ftank->env, f);
  fclose(f);
  if (ret) {
    fprintf(stderr, "Parse error in %s, character %d: %s\n",
            progpath,
            ret,
            forf_error_str[ftank->env.error]);
    return 0;
  }

  /* Back up the program so we can run it over and over without
     needing to re-parse */
  forf_stack_copy(&ftank->_prog, &ftank->_cmd);

  tank_init(tank, (tank_run_func *)ft_run_tank, ftank);

  return 1;
}

void
ft_tank_init(struct forftank         *ftank,
             struct tank             *tank,
             struct forf_lexical_env *lenv)
{
  /* Set up forf environment */
  forf_stack_init(&ftank->_prog, ftank->_progvals, CSTACK_SIZE);
  forf_stack_init(&ftank->_cmd, ftank->_cmdvals, CSTACK_SIZE);
  forf_stack_init(&ftank->_data, ftank->_datavals, DSTACK_SIZE);
  forf_memory_init(&ftank->_mem, ftank->_memvals, MEMORY_SIZE);
  forf_env_init(&ftank->env,
                lenv,
                &ftank->_data,
                &ftank->_cmd,
                &ftank->_mem,
                tank);
}

void
ft_read_sensors(struct tank *tank,
                char        *path)
{
  int i;

  for (i = 0; i < TANK_MAX_SENSORS; i += 1) {
    int   ret;
    char  fn[10];
    char  s[20];
    char *p = s;
    long  range;
    long  angle;
    long  width;
    long  turret;

    snprintf(fn, sizeof(fn), "sensor%d", i);
    ret = ft_read_file(s, sizeof(s), path, fn);
    if (! ret) {
      s[0] = 0;
    }
    range = strtol(p, &p, 0);
    angle = strtol(p, &p, 0);
    width = strtol(p, &p, 0);
    turret = strtol(p, &p, 0);

    tank->sensors[i].range = min(range, TANK_SENSOR_RANGE);
    tank->sensors[i].angle = deg2rad(angle % 360);
    tank->sensors[i].width = deg2rad(width % 360);
    tank->sensors[i].turret = (0 != turret);
  }
}

int
ft_read_tank(struct forftank         *ftank,
             struct tank             *tank,
             struct forf_lexical_env *lenv,
             char                    *path)
{
  int ret;

  /* What is your name? */
  ret = ft_read_file(ftank->name, sizeof(ftank->name), path, "name");
  if (! ret) {
    strncpy(ftank->name, path, sizeof(ftank->name));
  }

  /* What is your quest? */
  ft_tank_init(ftank, tank, lenv);
  ret = ft_read_program(ftank, tank, lenv, path);
  if (ret) {
    ft_read_sensors(tank, path);
  } else {
    /* Brick the tank */
    tank_init(tank, ft_bricked_tank, NULL);
  }

  /* What is your favorite color? */
  ret = ft_read_file(ftank->color, sizeof(ftank->color), path, "color");
  if (! ret) {
    strncpy(ftank->color, "808080", sizeof(ftank->color));
  }

  return 1;
}

void
print_header(FILE              *f,
             struct tanks_game *game,
             struct forftank   *forftanks,
             struct tank       *tanks,
             int                ntanks)
{
  int i, j;

  fprintf(f, "[[%d, %d, %d],[\n",
         (int)game->size[0], (int)game->size[1], TANK_CANNON_RANGE);
  for (i = 0; i < ntanks; i += 1) {
    fprintf(f, " [\"#%s\",[", forftanks[i].color);
    for (j = 0; j < TANK_MAX_SENSORS; j += 1) {
      struct sensor *s = &(tanks[i].sensors[j]);

      if (! s->range) {
        continue;
      }
      fprintf(f, "[%d, %.2f, %.2f, %d],",
             (int)(s->range),
             s->angle,
             s->width,
             s->turret);
    }
    fprintf(f, "]],\n");
  }
  fprintf(f, "],[\n");
}

void
print_footer(FILE *f)
{
  fprintf(f, "]]\n");
}

int
main(int argc, char *argv[])
{
  struct tanks_game       game;
  struct forftank         myftanks[MAX_TANKS];
  struct tank             mytanks[MAX_TANKS];
  struct forf_lexical_env lenv[LENV_SIZE];
  int                     order[MAX_TANKS];
  int                     ntanks = 0;
  int                     i;
  int                     alive;

  lenv[0].name = NULL;
  lenv[0].proc = NULL;
  if ((! forf_extend_lexical_env(lenv, forf_base_lexical_env, LENV_SIZE)) ||
      (! forf_extend_lexical_env(lenv, tanks_lenv_addons, LENV_SIZE))) {
    fprintf(stderr, "Unable to initialize lexical environment.\n");
    return 1;
  }

  /* We only need slightly random numbers */
  {
    char *s    = getenv("SEED");
    int   seed = atoi(s?s:"");

    if (! seed) {
      seed = getpid();
    }

    srand(seed);
    fprintf(stdout, "// SEED=%d\n", seed);
  }

  /* Shuffle the order we read things in */
  for (i = 0; i < MAX_TANKS; i += 1) {
    order[i] = i;
  }
  for (i = 0; i < argc - 1; i += 1) {
    int j = rand() % (argc - 1);
    int n = order[j];

    order[j] = order[i];
    order[i] = n;
  }

  /* Every argument is a tank directory */
  for (i = 0; i < argc - 1; i += 1) {
    if (ft_read_tank(&myftanks[ntanks],
                     &mytanks[ntanks],
                     lenv,
                     argv[order[i] + 1])) {
      ntanks += 1;
    }
  }

  if (0 == ntanks) {
    fprintf(stderr, "No usable tanks!\n");
    return 1;
  }

  /* Calculate the size of the game board */
  {
    int x, y;

    for (x = 1; x * x < ntanks; x += 1);
    y = ntanks / x;
    if (ntanks % x) {
      y += 1;
    }

    game.size[0] = x * SPACING;
    game.size[1] = y * SPACING;
  }

  /* Position tanks */
  {
    int x = 50;
    int y = 50;

    for (i = 0; i < ntanks; i += 1) {
      mytanks[i].position[0] = (float)x;
      mytanks[i].position[1] = (float)y;
      mytanks[i].angle = deg2rad(rand() % 360);
      mytanks[i].turret.current = deg2rad(rand() % 360);

      x += SPACING;
      if (x > game.size[0]) {
        x %= (int)(game.size[0]);
        y += SPACING;
      }
    }
  }

  print_header(stdout, &game, myftanks, mytanks, ntanks);

  /* Run rounds */
  alive = ntanks;
  for (i = 0; (alive > 1) && (i < ROUNDS); i += 1) {
    int j;

    tanks_run_turn(&game, mytanks, ntanks);
    fprintf(stdout, "[\n");
    alive = ntanks;
    for (j = 0; j < ntanks; j += 1) {
      struct tank *t = &(mytanks[j]);

      if (t->killer) {
        alive -= 1;
        fprintf(stdout, " 0,\n");
      } else {
        int k;
        int flags   = 0;
        int sensors = 0;

        for (k = 0; k < TANK_MAX_SENSORS; k += 1) {
          if (t->sensors[k].triggered) {
            sensors |= (1 << k);
          }
        }
        if (t->turret.firing) {
          flags |= 1;
        }
        if (t->led) {
          flags |= 2;
        }
        fprintf(stdout, " [%d,%d,%.2f,%.2f,%d,%d],\n",
               (int)(t->position[0]),
               (int)(t->position[1]),
               t->angle,
               t->turret.current,
               flags,
               sensors);
      }
    }
    fprintf(stdout, "],\n");
  }

  print_footer(stdout);

  return 0;
}