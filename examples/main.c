#include <stdio.h>

enum Color { RED = 1, GREEN = 2, BLUE = 4 };

enum Direction { NORTH, SOUTH, EAST, WEST };

const char *direction_to_string(enum Direction direction) {
  switch (direction) {
  case NORTH:
    return "North";
  case SOUTH:
    return "South";
  case EAST:
    return "East";
  case WEST:
    return "West";
  default:
    return "Unknown";
  }
}

int main(void) {
  enum Color my_color = RED;

  printf("%d\n", my_color);                   // 1
  printf("%d\n", GREEN);                      // 2
  printf("%s\n", direction_to_string(NORTH)); // North

  return 0;
}
