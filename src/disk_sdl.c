/********************************************************************/
/* disk_sdl.c - Direct asset loading for SDL2 (replaces .dat files) */
/********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#include "sdl_compat.h"
#include "disk_sdl.h"
#include "map.h"
#include "texture.h"
#include "log.h"

static void
lw_safe_strcpy (char *dst, const char *src, size_t dstsize)
{
  if (dstsize == 0)
    return;
  size_t len = strlen (src);
  if (len >= dstsize)
    len = dstsize - 1;
  memcpy (dst, src, len);
  dst[len] = '\0';
}

/*==================================================================*/
/* Helper to find data directory                                     */
/*==================================================================*/

static char lw_data_dir[512] = "";

void
lw_disk_sdl_set_data_dir (const char *dat_path)
{
  const char *last_slash;

  if (!dat_path)
    return;

  /* Derive data dir from the .dat path by going up one level */
  lw_safe_strcpy (lw_data_dir, dat_path, sizeof (lw_data_dir));
  last_slash = strrchr (lw_data_dir, '/');
  if (last_slash)
    lw_data_dir[last_slash - lw_data_dir] = '\0';
  else
    lw_safe_strcpy (lw_data_dir, ".", sizeof (lw_data_dir));
}

static void
build_subdir_path (char *out, size_t outsize, const char *subdir)
{
  snprintf (out, outsize, "%s/%s", lw_data_dir, subdir);
}

/*==================================================================*/
/* Load maps (PCX/BMP files from data/map/)                         */
/*==================================================================*/

int
lw_disk_sdl_load_maps (void **maps, int max_maps)
{
  char dir_path[1024];
  DIR *d;
  struct dirent *entry;
  int count = 0;

  build_subdir_path (dir_path, sizeof (dir_path), "map");
  d = opendir (dir_path);
  if (!d)
    return 0;

  while ((entry = readdir (d)) != NULL && count < max_maps)
    {
      char *ext;
      char file_path[1280];
      void *raw;

      ext = strrchr (entry->d_name, '.');
      if (!ext)
        continue;
      if (strcasecmp (ext, ".pcx") != 0 && strcasecmp (ext, ".bmp") != 0)
        continue;

      snprintf (file_path, sizeof (file_path), "%s/%s", dir_path,
                entry->d_name);
      raw = lw_map_archive_raw (file_path);
      if (raw)
        {
          maps[count] = raw;
          count++;
        }
    }

  closedir (d);
  return count;
}

/*==================================================================*/
/* Load sound effects (WAV files from data/sfx/)                    */
/*==================================================================*/

int
lw_disk_sdl_load_sfx (SAMPLE ** samples, int max_samples, const char *subdir)
{
  char dir_path[1024];
  DIR *d;
  struct dirent *entry;
  int count = 0;

  build_subdir_path (dir_path, sizeof (dir_path), subdir);
  d = opendir (dir_path);
  if (!d)
    return 0;

  while ((entry = readdir (d)) != NULL && count < max_samples)
    {
      char *ext;
      char file_path[1280];
      Mix_Chunk *chunk;

      ext = strrchr (entry->d_name, '.');
      if (!ext)
        continue;
      if (strcasecmp (ext, ".wav") != 0)
        continue;

      snprintf (file_path, sizeof (file_path), "%s/%s", dir_path,
                entry->d_name);
      chunk = Mix_LoadWAV (file_path);
      if (chunk)
        {
          samples[count] = (SAMPLE *) chunk;
          count++;
        }
    }

  closedir (d);
  return count;
}

/*==================================================================*/
/* Load MIDI music files from data/music/                           */
/*==================================================================*/

int
lw_disk_sdl_load_music (MIDI ** music, int max_music)
{
  char dir_path[1024];
  DIR *d;
  struct dirent *entry;
  int count = 0;

  build_subdir_path (dir_path, sizeof (dir_path), "music");
  d = opendir (dir_path);
  if (!d)
    return 0;

  while ((entry = readdir (d)) != NULL && count < max_music)
    {
      char *ext;
      char file_path[1280];
      Mix_Music *mus;

      ext = strrchr (entry->d_name, '.');
      if (!ext)
        continue;
      if (strcasecmp (ext, ".mid") != 0 && strcasecmp (ext, ".ogg") != 0
          && strcasecmp (ext, ".mp3") != 0)
        continue;

      snprintf (file_path, sizeof (file_path), "%s/%s", dir_path,
                entry->d_name);
      mus = Mix_LoadMUS (file_path);
      if (mus)
        {
          music[count] = (MIDI *) mus;
          count++;
        }
    }

  closedir (d);
  return count;
}

/*==================================================================*/
/* Load textures (PCX files from data/texture/ or data/maptex/)     */
/*==================================================================*/

int
lw_disk_sdl_load_textures (void **textures, int max_textures,
                            const char *subdir)
{
  char dir_path[1024];
  DIR *d;
  struct dirent *entry;
  int count = 0;

  build_subdir_path (dir_path, sizeof (dir_path), subdir);
  d = opendir (dir_path);
  if (!d)
    return 0;

  while ((entry = readdir (d)) != NULL && count < max_textures)
    {
      char *ext;
      char file_path[1280];
      void *raw;

      ext = strrchr (entry->d_name, '.');
      if (!ext)
        continue;
      if (strcasecmp (ext, ".pcx") != 0 && strcasecmp (ext, ".bmp") != 0)
        continue;

      snprintf (file_path, sizeof (file_path), "%s/%s", dir_path,
                entry->d_name);
      raw = lw_texture_archive_raw (file_path);
      if (raw)
        {
          textures[count] = raw;
          count++;
        }
    }

  closedir (d);
  return count;
}

/*==================================================================*/
/* Load background image                                             */
/*==================================================================*/

BITMAP *
lw_disk_sdl_load_back (void)
{
  char file_path[1024];

  build_subdir_path (file_path, sizeof (file_path), "back/lw5back.pcx");
  return load_bitmap (file_path, NULL);
}

/*==================================================================*/
/* Load font bitmaps                                                 */
/*==================================================================*/

BITMAP *
lw_disk_sdl_load_font_bitmap (const char *name)
{
  char file_path[1024];

  snprintf (file_path, sizeof (file_path), "%s/font/%s", lw_data_dir, name);
  return load_bitmap (file_path, NULL);
}
