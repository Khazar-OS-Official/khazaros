#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>

static void mode_to_string(uint16_t mode, char *str, int is_dir) {
  str[0] = is_dir ? 'd' : '-';
  str[1] = (mode & 0x100) ? 'r' : '-'; // S_IRUSR
  str[2] = (mode & 0x080) ? 'w' : '-'; // S_IWUSR
  str[3] = (mode & 0x040) ? 'x' : '-'; // S_IXUSR
  str[4] = (mode & 0x020) ? 'r' : '-'; // S_IRGRP
  str[5] = (mode & 0x010) ? 'w' : '-'; // S_IWGRP
  str[6] = (mode & 0x008) ? 'x' : '-'; // S_IXGRP
  str[7] = (mode & 0x004) ? 'r' : '-'; // S_IROTH
  str[8] = (mode & 0x002) ? 'w' : '-'; // S_IWOTH
  str[9] = (mode & 0x001) ? 'x' : '-'; // S_IXOTH
  str[10] = '\0';
}

int main(int argc, char **argv) {
  const char *path = "/";
  if (argc > 1) {
    path = argv[1];
  }

  DIR *dir = opendir(path);
  if (!dir) {
    printf("Error: Cannot open directory '%s'\n", path);
    return 1;
  }

  dirent_t *entry;
  char mode_str[11];
  while ((entry = readdir(dir)) != NULL) {
    mode_to_string(entry->d_mode, mode_str, (entry->d_type == 2));
    printf("%s  %s  %d bytes\n", mode_str, entry->d_name, entry->d_off);
  }

  closedir(dir);
  return 0;
}
