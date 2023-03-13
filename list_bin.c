#include <dirent.h>
#include <stdio.h>
#include <string.h>

char **find_bin_files() {
  DIR *dir;
  struct dirent *ent;
  int i = 0;
  static char
      *bin_files[100]; // Assume there are at most 100 .bin files in directory

  dir = opendir("./binaries/");
  if (dir != NULL) {
    while ((ent = readdir(dir)) != NULL) {
      if (strstr(ent->d_name, ".bin") !=
          NULL) { // Check if file name contains .bin
        bin_files[i] = ent->d_name;
        i++;
      }
    }
    closedir(dir);
  } else {
    printf("Could not open directory");
    return NULL;
  }

  // Add null terminator to array
  bin_files[i] = NULL;

  return bin_files;
}
