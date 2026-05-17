#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
  if (argc != 4) {
    printf("Usage: calc <num1> <op> <num2>\n");
    printf("Example: calc 5 + 3\n");
    printf("Operators: +, -, *, /\n");
    return 1;
  }

  int a = atoi(argv[1]);
  char op = argv[2][0];
  int b = atoi(argv[3]);
  int res = 0;

  if (op == '+') {
    res = a + b;
  } else if (op == '-') {
    res = a - b;
  } else if (op == '*') {
    res = a * b;
  } else if (op == '/') {
    if (b == 0) {
      printf("Error: Division by zero\n");
      return 1;
    }
    res = a / b;
  } else {
    printf("Error: Unknown operator '%c'\n", op);
    return 1;
  }

  printf("%d %c %d = %d\n", a, op, b, res);
  return 0;
}
