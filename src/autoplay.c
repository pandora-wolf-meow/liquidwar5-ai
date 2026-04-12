/********************************************************************/
/*                                                                  */
/*            L   I  QQ  U U I DD    W   W  A  RR    555            */
/*            L   I Q  Q U U I D D   W   W A A R R   5              */
/*            L   I Q  Q U U I D D   W W W AAA RR    55             */
/*            L   I Q Q  U U I D D   WW WW A A R R     5            */
/*            LLL I  Q Q  U  I DD    W   W A A R R   55             */
/*                                                                  */
/*                             b                                    */
/*                             bb  y y                              */
/*                             b b yyy                              */
/*                             bb    y                              */
/*                                 yy                               */
/*                                                                  */
/*                     U U       FFF  O   O  TTT                    */
/*                     U U       F   O O O O  T                     */
/*                     U U TIRET FF  O O O O  T                     */
/*                     U U       F   O O O O  T                     */
/*                      U        F    O   O   T                     */
/*                                                                  */
/********************************************************************/

/*****************************************************************************/
/* Liquid War is a multiplayer wargame                                       */
/* Copyright (C) 1998-2025 Christian Mauduit                                 */
/*                                                                           */
/* This program is free software; you can redistribute it and/or modify      */
/* it under the terms of the GNU General Public License as published by      */
/* the Free Software Foundation; either version 2 of the License, or         */
/* (at your option) any later version.                                       */
/*                                                                           */
/* This program is distributed in the hope that it will be useful,           */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of            */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             */
/* GNU General Public License for more details.                              */
/*                                                                           */
/* You should have received a copy of the GNU General Public License         */
/* along with this program; if not, write to the Free Software               */
/* Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */
/*                                                                           */
/* Liquid War homepage : https://ufoot.org/liquidwar/v5                   */
/* Contact author      : ufoot@ufoot.org                                     */
/*****************************************************************************/

/********************************************************************/
/* nom           : autoplay.c                                       */
/* contenu       : l'ordino joue tout seul                          */
/* date de modif : 3 mai 98                                         */
/********************************************************************/

/*==================================================================*/
/* includes                                                         */
/*==================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "army.h"
#include "autoplay.h"
#include "cursor.h"
#include "fighter.h"
#include "mesh.h"
#include "move.h"
#include "lwtime.h"
#include "config.h"

/*==================================================================*/
/* constants                                                        */
/*==================================================================*/

#define LW_AUTOPLAY_RANDOM_LIMIT 10000
#define LW_AUTOPLAY_FIGHTER_TRACK_INTERVAL 30

/*==================================================================*/
/* tunable AI parameters — per team, settable via command line       */
/*==================================================================*/

int LW_AI_NUM_CANDIDATES = 10;
int LW_AI_DENSITY_RADIUS = 5;
int LW_AI_DENSITY_WEIGHT = 50;
int LW_AI_HEALTH_WEIGHT = 100;
int LW_AI_REPLAN_INTERVAL = 50;
int LW_AI_RETREAT_RATIO = 20;

LW_AI_PARAMS LW_AI_TEAM_PARAMS[NB_TEAMS];

/*------------------------------------------------------------------*/
void
lw_ai_init_params (void)
{
  int i;
  for (i = 0; i < NB_TEAMS; i++)
    {
      LW_AI_TEAM_PARAMS[i].candidates = LW_AI_NUM_CANDIDATES;
      LW_AI_TEAM_PARAMS[i].density_radius = LW_AI_DENSITY_RADIUS;
      LW_AI_TEAM_PARAMS[i].density_weight = LW_AI_DENSITY_WEIGHT;
      LW_AI_TEAM_PARAMS[i].health_weight = LW_AI_HEALTH_WEIGHT;
      LW_AI_TEAM_PARAMS[i].replan = LW_AI_REPLAN_INTERVAL;
      LW_AI_TEAM_PARAMS[i].retreat = LW_AI_RETREAT_RATIO;
    }
}

/*------------------------------------------------------------------*/
int
lw_ai_load_params_file (const char *path)
{
  FILE *fp;
  int team, val;
  char key[64];

  fp = fopen (path, "r");
  if (!fp)
    return 0;

  /*
   * Simple format: one line per setting.
   * "team candidates 0 15"  means team 0 candidates = 15
   * "team density_weight 2 300" means team 2 density_weight = 300
   */
  while (fscanf (fp, "%63s %d %d", key, &team, &val) == 3)
    {
      if (team < 0 || team >= NB_TEAMS)
        continue;
      if (strcmp (key, "candidates") == 0)
        LW_AI_TEAM_PARAMS[team].candidates = val;
      else if (strcmp (key, "density_radius") == 0)
        LW_AI_TEAM_PARAMS[team].density_radius = val;
      else if (strcmp (key, "density_weight") == 0)
        LW_AI_TEAM_PARAMS[team].density_weight = val;
      else if (strcmp (key, "health_weight") == 0)
        LW_AI_TEAM_PARAMS[team].health_weight = val;
      else if (strcmp (key, "replan") == 0)
        LW_AI_TEAM_PARAMS[team].replan = val;
      else if (strcmp (key, "retreat") == 0)
        LW_AI_TEAM_PARAMS[team].retreat = val;
    }

  fclose (fp);
  return 1;
}

/*==================================================================*/
/* variables globales                                               */
/*==================================================================*/

static char COMPUTER_PATH_KEYS[NB_TEAMS][COMPUTER_PATH_MAX];
static int COMPUTER_PATH_SIZE[NB_TEAMS];
static int COMPUTER_PATH_WAIT[NB_TEAMS];
static int COMPUTER_TEAM_FIGHTERS[NB_TEAMS];
static int COMPUTER_TEAM_FIGHTERS_PREV[NB_TEAMS];
static int COMPUTER_FIGHTERS_LAST_CLOCK = -999;

/*==================================================================*/
/* forward declarations                                             */
/*==================================================================*/

static void count_all_team_fighters (int *counts);

/*==================================================================*/
/* battle data logger                                               */
/*==================================================================*/

#define LW_LOG_STATE_INTERVAL 20

static FILE *LOG_STATE_FILE = NULL;
static FILE *LOG_DECISION_FILE = NULL;
static int LOG_STATE_LAST_CLOCK = -999;

static void
battle_log_init (void)
{
  char filename[256];
  time_t now;
  struct tm *t;

  if (LOG_STATE_FILE)
    fclose (LOG_STATE_FILE);
  if (LOG_DECISION_FILE)
    fclose (LOG_DECISION_FILE);

  now = time (NULL);
  t = localtime (&now);

  snprintf (filename, sizeof (filename),
            "battle_state_%04d%02d%02d_%02d%02d%02d.csv",
            t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
            t->tm_hour, t->tm_min, t->tm_sec);
  LOG_STATE_FILE = fopen (filename, "w");
  if (LOG_STATE_FILE)
    {
      fprintf (LOG_STATE_FILE,
               "tick,team0_fighters,team1_fighters,team2_fighters,"
               "team3_fighters,team4_fighters,team5_fighters,"
               "cursor0_x,cursor0_y,cursor1_x,cursor1_y,"
               "cursor2_x,cursor2_y,cursor3_x,cursor3_y,"
               "cursor4_x,cursor4_y,cursor5_x,cursor5_y,"
               "area_w,area_h,army_size\n");
      fprintf (stderr, "[LOG] State log: %s\n", filename);
    }

  snprintf (filename, sizeof (filename),
            "battle_decisions_%04d%02d%02d_%02d%02d%02d.csv",
            t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
            t->tm_hour, t->tm_min, t->tm_sec);
  LOG_DECISION_FILE = fopen (filename, "w");
  if (LOG_DECISION_FILE)
    {
      fprintf (LOG_DECISION_FILE,
               "tick,team,decision,target_x,target_y,best_score,"
               "cursor_x,cursor_y,own_fighters,own_fighters_prev,"
               "cand_scores\n");
      fprintf (stderr, "[LOG] Decision log: %s\n", filename);
    }
}

static void
battle_log_state (void)
{
  int counts[NB_TEAMS];
  int i;

  if (!LOG_STATE_FILE)
    return;
  if (GLOBAL_CLOCK - LOG_STATE_LAST_CLOCK < LW_LOG_STATE_INTERVAL)
    return;

  LOG_STATE_LAST_CLOCK = GLOBAL_CLOCK;
  count_all_team_fighters (counts);

  fprintf (LOG_STATE_FILE, "%d", GLOBAL_CLOCK);
  for (i = 0; i < NB_TEAMS; i++)
    fprintf (LOG_STATE_FILE, ",%d", counts[i]);
  for (i = 0; i < NB_TEAMS; i++)
    {
      if (CURRENT_CURSOR[i].active)
        fprintf (LOG_STATE_FILE, ",%d,%d",
                 CURRENT_CURSOR[i].x, CURRENT_CURSOR[i].y);
      else
        fprintf (LOG_STATE_FILE, ",-1,-1");
    }
  fprintf (LOG_STATE_FILE, ",%d,%d,%d\n",
           CURRENT_AREA_W, CURRENT_AREA_H, CURRENT_ARMY_SIZE);
  fflush (LOG_STATE_FILE);
}

static void
battle_log_decision (int team, const char *decision,
                     int target_x, int target_y, int best_score,
                     const char *cand_scores_str)
{
  if (!LOG_DECISION_FILE)
    return;

  fprintf (LOG_DECISION_FILE, "%d,%d,%s,%d,%d,%d,%d,%d,%d,%d,%s\n",
           GLOBAL_CLOCK, team, decision, target_x, target_y, best_score,
           CURRENT_CURSOR[team].x, CURRENT_CURSOR[team].y,
           COMPUTER_TEAM_FIGHTERS[team],
           COMPUTER_TEAM_FIGHTERS_PREV[team],
           cand_scores_str);
  fflush (LOG_DECISION_FILE);
}

static void
battle_log_close (void)
{
  if (LOG_STATE_FILE)
    {
      fclose (LOG_STATE_FILE);
      LOG_STATE_FILE = NULL;
    }
  if (LOG_DECISION_FILE)
    {
      fclose (LOG_DECISION_FILE);
      LOG_DECISION_FILE = NULL;
    }
}

/*==================================================================*/
/* fonctions                                                        */
/*==================================================================*/

/*------------------------------------------------------------------*/
static int
count_nearby_enemies (int cx, int cy, int my_team, int radius)
{
  int count = 0;
  int x, y;
  FIGHTER *f;

  for (y = cy - radius; y <= cy + radius; y++)
    {
      if (y < 0 || y >= CURRENT_AREA_H)
        continue;
      for (x = cx - radius; x <= cx + radius; x++)
        {
          if (x < 0 || x >= CURRENT_AREA_W)
            continue;
          f = CURRENT_AREA[y * CURRENT_AREA_W + x].fighter;
          if (f && f->team != my_team)
            count++;
        }
    }
  return count;
}

/*------------------------------------------------------------------*/
static int
score_candidate (int cx, int cy, int health,
                 int cursor_x, int cursor_y, int my_team)
{
  int dist, density, health_score;

  dist = abs (cx - cursor_x) + abs (cy - cursor_y);
  density = count_nearby_enemies (cx, cy, my_team,
                                  LW_AI_TEAM_PARAMS[my_team].density_radius);
  health_score = (MAX_FIGHTER_HEALTH - health);

  return density * LW_AI_TEAM_PARAMS[my_team].density_weight
    - dist + health_score / LW_AI_TEAM_PARAMS[my_team].health_weight;
}

/*------------------------------------------------------------------*/
static void
count_all_team_fighters (int *counts)
{
  int i;

  for (i = 0; i < NB_TEAMS; i++)
    counts[i] = 0;

  for (i = 0; i < CURRENT_ARMY_SIZE; i++)
    counts[(int) CURRENT_ARMY[i].team]++;
}

/*------------------------------------------------------------------*/
static void
find_team_centroid (int team, int *cx, int *cy)
{
  long sum_x = 0, sum_y = 0;
  int count = 0;
  int i;

  for (i = 0; i < CURRENT_ARMY_SIZE; i++)
    {
      if (CURRENT_ARMY[i].team == team)
        {
          sum_x += CURRENT_ARMY[i].x;
          sum_y += CURRENT_ARMY[i].y;
          count++;
        }
    }

  if (count > 0)
    {
      *cx = (int) (sum_x / count);
      *cy = (int) (sum_y / count);
    }
  else
    {
      *cx = CURRENT_AREA_W / 2;
      *cy = CURRENT_AREA_H / 2;
    }
}

/*------------------------------------------------------------------*/
static void
update_fighter_tracking (void)
{
  int i;

  if (GLOBAL_CLOCK - COMPUTER_FIGHTERS_LAST_CLOCK
      >= LW_AUTOPLAY_FIGHTER_TRACK_INTERVAL)
    {
      COMPUTER_FIGHTERS_LAST_CLOCK = GLOBAL_CLOCK;
      for (i = 0; i < NB_TEAMS; i++)
        COMPUTER_TEAM_FIGHTERS_PREV[i] = COMPUTER_TEAM_FIGHTERS[i];
      count_all_team_fighters (COMPUTER_TEAM_FIGHTERS);
    }
}

/*------------------------------------------------------------------*/
static void
calculate_computer_path (int dst_x, int dst_y, int cursor)
{
  int team, src_x, src_y, pos, dir, x, y, dx, dy;
  MESH *path_mesh, *src_mesh;
  int sens, start;
  int table;

  table = GLOBAL_CLOCK % 2;

  team = CURRENT_CURSOR[cursor].team;

  src_x = CURRENT_CURSOR[cursor].x;
  src_y = CURRENT_CURSOR[cursor].y;
  src_mesh = CURRENT_AREA[src_y * CURRENT_AREA_W + src_x].mesh;

  x = dst_x;
  y = dst_y;
  path_mesh = CURRENT_AREA[y * CURRENT_AREA_W + x].mesh;
  pos = sens = start = 0;
  while (pos < COMPUTER_PATH_MAX && path_mesh != 0 && path_mesh != src_mesh)
    {
      dir = get_main_dir (path_mesh, team, sens, start);

      x += (dx = FIGHTER_MOVE_X[table][dir][0]);
      y += (dy = FIGHTER_MOVE_Y[table][dir][0]);

      path_mesh = CURRENT_AREA[y * CURRENT_AREA_W + x].mesh;
      if (path_mesh)
        {
          COMPUTER_PATH_KEYS[cursor][pos] = 0;
          if (dy > 0)
            COMPUTER_PATH_KEYS[cursor][pos] += CURSOR_KEY_UP;
          if (dx < 0)
            COMPUTER_PATH_KEYS[cursor][pos] += CURSOR_KEY_RIGHT;
          if (dy < 0)
            COMPUTER_PATH_KEYS[cursor][pos] += CURSOR_KEY_DOWN;
          if (dx > 0)
            COMPUTER_PATH_KEYS[cursor][pos] += CURSOR_KEY_LEFT;
          pos++;
        }
      start++;
      if (start == NB_DIRS)
        {
          start = 0;
          sens = !sens;
        }
    }

  if (path_mesh == src_mesh)
    {
      while (pos < COMPUTER_PATH_MAX && (x != src_x || y != src_y))
        {
          COMPUTER_PATH_KEYS[cursor][pos] = 0;

          if (y < src_y)
            {
              y++;
              COMPUTER_PATH_KEYS[cursor][pos] += CURSOR_KEY_UP;
            }
          if (x > src_x)
            {
              x--;
              COMPUTER_PATH_KEYS[cursor][pos] += CURSOR_KEY_RIGHT;
            }
          if (y > src_y)
            {
              y--;
              COMPUTER_PATH_KEYS[cursor][pos] += CURSOR_KEY_DOWN;
            }
          if (x < src_x)
            {
              x++;
              COMPUTER_PATH_KEYS[cursor][pos] += CURSOR_KEY_LEFT;
            }
          pos++;
        }
      COMPUTER_PATH_SIZE[cursor] = pos;
    }
  else
    COMPUTER_PATH_SIZE[cursor] = 0;

  COMPUTER_PATH_WAIT[cursor] = COMPUTER_PATH_SIZE[cursor];
}

/*------------------------------------------------------------------*/
void
reset_computer_path (void)
{
  int i;

  for (i = 0; i < NB_TEAMS; ++i)
    {
      COMPUTER_PATH_SIZE[i] = 0;
      COMPUTER_PATH_WAIT[i] = 0;
      COMPUTER_TEAM_FIGHTERS[i] = 0;
      COMPUTER_TEAM_FIGHTERS_PREV[i] = 0;
    }
  COMPUTER_FIGHTERS_LAST_CLOCK = -999;
  battle_log_init ();
}

/*------------------------------------------------------------------*/
void
close_computer_path (void)
{
  battle_log_close ();
}

/*------------------------------------------------------------------*/
/*
 * Returns true if the given fighter index is a valid target for the
 * given team, respecting the control_type preference (cpu_vs_human).
 */
static int
is_valid_target (int idx, int team, int control_type,
                 int *control_type_array)
{
  return (CURRENT_ARMY[idx].team != team) &&
    (control_type_array[(int) (CURRENT_ARMY[idx].team)] == control_type
     || control_type == CONFIG_CONTROL_TYPE_OFF);
}

/*------------------------------------------------------------------*/
/*
 * Selects the best target for a computer cursor by sampling multiple
 * candidates and scoring them based on enemy density, proximity to
 * the cursor, and fighter health.
 */
static int
scored_target_selection (int *x, int *y, int team, int cursor,
                         char *scores_out, int scores_out_size)
{
  int control_type = 0;
  int control_type_array[NB_TEAMS];
  int best_x, best_y, best_score;
  int cursor_x, cursor_y;
  int i, j, found, idx;
  int cand_score;
  int num_scored = 0;
  int pos = 0;

  scores_out[0] = '\0';

  switch (LW_CONFIG_CURRENT_RULES.cpu_vs_human)
    {
    case CONFIG_CPU_VS_HUMAN_ALWAYS:
      control_type = CONFIG_CONTROL_TYPE_HUMAN;
      break;
    case CONFIG_CPU_VS_HUMAN_NEVER:
      control_type = CONFIG_CONTROL_TYPE_CPU;
      break;
    default:
      control_type = CONFIG_CONTROL_TYPE_OFF;
      break;
    }

  for (i = 0; i < NB_TEAMS; ++i)
    control_type_array[i] = CONFIG_CONTROL_TYPE_OFF;

  for (i = 0; i < NB_TEAMS; ++i)
    {
      if ((CURRENT_CURSOR[i].control_type == control_type
           || control_type == CONFIG_CONTROL_TYPE_OFF)
          && CURRENT_CURSOR[i].active)
        control_type_array[CURRENT_CURSOR[i].team] = control_type;
    }

  cursor_x = CURRENT_CURSOR[cursor].x;
  cursor_y = CURRENT_CURSOR[cursor].y;

  best_score = -999999;
  best_x = -1;
  best_y = -1;

  for (j = 0; j < LW_AI_TEAM_PARAMS[team].candidates; j++)
    {
      found = 0;
      for (i = 0; i < 100 && !found; i++)
        {
          idx = random () % CURRENT_ARMY_SIZE;
          if (is_valid_target (idx, team, control_type, control_type_array))
            {
              cand_score =
                score_candidate (CURRENT_ARMY[idx].x, CURRENT_ARMY[idx].y,
                                 CURRENT_ARMY[idx].health, cursor_x, cursor_y,
                                 team);
              if (pos < scores_out_size - 20)
                pos += snprintf (scores_out + pos, scores_out_size - pos,
                                 "%s%d@%d;%d",
                                 num_scored > 0 ? "|" : "",
                                 cand_score,
                                 CURRENT_ARMY[idx].x, CURRENT_ARMY[idx].y);
              num_scored++;
              if (cand_score > best_score)
                {
                  best_score = cand_score;
                  best_x = CURRENT_ARMY[idx].x;
                  best_y = CURRENT_ARMY[idx].y;
                }
              found = 1;
            }
        }
    }

  if (best_x >= 0)
    {
      *x = best_x;
      *y = best_y;
    }
  else
    {
      for (j = 0; j < LW_AI_TEAM_PARAMS[team].candidates; j++)
        {
          idx = random () % CURRENT_ARMY_SIZE;
          if (CURRENT_ARMY[idx].team != team)
            {
              cand_score =
                score_candidate (CURRENT_ARMY[idx].x, CURRENT_ARMY[idx].y,
                                 CURRENT_ARMY[idx].health, cursor_x, cursor_y,
                                 team);
              if (cand_score > best_score)
                {
                  best_score = cand_score;
                  best_x = CURRENT_ARMY[idx].x;
                  best_y = CURRENT_ARMY[idx].y;
                }
            }
        }
      if (best_x >= 0)
        {
          *x = best_x;
          *y = best_y;
        }
      else
        {
          idx = random () % CURRENT_ARMY_SIZE;
          *x = CURRENT_ARMY[idx].x;
          *y = CURRENT_ARMY[idx].y;
          best_score = 0;
        }
    }
  return best_score;
}

/*------------------------------------------------------------------*/
char
get_computer_next_move (int cursor)
{
  FIGHTER *f;
  char key_info;
  int x, y;
  int meme_equipe, team;
  int losing_fighters;
  int best_score;
  char cand_scores[512];

  team = CURRENT_CURSOR[cursor].team;

  update_fighter_tracking ();
  battle_log_state ();

  if (COMPUTER_PATH_SIZE[cursor] > 0)
    {
      if (LW_AI_TEAM_PARAMS[team].replan > 0
          && GLOBAL_CLOCK % LW_AI_TEAM_PARAMS[team].replan == 0)
        COMPUTER_PATH_SIZE[cursor] = 0;
      else
        return COMPUTER_PATH_KEYS[cursor][--COMPUTER_PATH_SIZE[cursor]];
    }

  key_info = 0;

  f = CURRENT_AREA[CURRENT_CURSOR[cursor].y * CURRENT_AREA_W
                    + CURRENT_CURSOR[cursor].x].fighter;
  if (f)
    meme_equipe = (f->team == team);
  else
    meme_equipe = 1;

  if ((--COMPUTER_PATH_WAIT[cursor]) < 0 || meme_equipe)
    {
      losing_fighters = (COMPUTER_TEAM_FIGHTERS_PREV[team] > 0
                         && (COMPUTER_TEAM_FIGHTERS_PREV[team]
                             - COMPUTER_TEAM_FIGHTERS[team])
                         > COMPUTER_TEAM_FIGHTERS_PREV[team]
                         / LW_AI_TEAM_PARAMS[team].retreat);

      if (losing_fighters)
        {
          find_team_centroid (team, &x, &y);
          battle_log_decision (team, "retreat", x, y, 0, "");
        }
      else
        {
          best_score = scored_target_selection (&x, &y, team, cursor,
                                                cand_scores,
                                                sizeof (cand_scores));
          battle_log_decision (team, "attack", x, y, best_score,
                               cand_scores);
        }

      calculate_computer_path (x, y, cursor);
    }

  return key_info;
}
